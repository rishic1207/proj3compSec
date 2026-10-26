/*
 * CS 4264 Project 3 - Objective 4: grammar-based, mutational,
 * coverage-guided greybox fuzzer
 *
 * Loop:
 *   1. Pick a seed from the seed pool (uniformly at random). If the pool is
 *      empty, generate a fresh input from the grammar instead.
 *   2. Mutate it: replace one randomly chosen position with a random grammar
 *      character, so every test case stays in [0-9a-zA-Z]{3}.
 *   3. Run the SUT (built with trace-pc-guard + cov_handlers.c) with system().
 *   4. Crash -> report and stop. Otherwise read the coverage file; if the run
 *      hit any edge never seen before, add the test case to the seed pool.
 *
 * Usage: ./fuzzer [target_path] [rng_seed]
 *   target_path defaults to ./targetCov
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#define INPUT_LEN      3
#define MAX_SEEDS      100        /* spec: assume at most 100 seeds needed */
#define MAX_EDGES      65536      /* must match cov_handlers.c             */
#define COV_FILE       "cov.bin"
#define PROGRESS_EVERY 10000ULL

/* ---------- grammar: <char> ::= lowercase | uppercase | digit ---------- */
static const char CHARSET[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
#define CHARSET_SIZE (sizeof(CHARSET) - 1)   /* 62 */

static char gen_char(void) { return CHARSET[rand() % CHARSET_SIZE]; }

static void gen_input(char *buf)              /* <input> ::= <char><char><char> */
{
    for (int i = 0; i < INPUT_LEN; i++)
        buf[i] = gen_char();
    buf[INPUT_LEN] = '\0';
}

/* ---------- seed pool ---------- */
static char seeds[MAX_SEEDS][INPUT_LEN + 1];
static int  num_seeds;

static void add_seed(const char *input)
{
    if (num_seeds < MAX_SEEDS) {
        memcpy(seeds[num_seeds++], input, INPUT_LEN + 1);
    } else {
        /* Shouldn't happen for this SUT; replace a random seed if it does. */
        memcpy(seeds[rand() % MAX_SEEDS], input, INPUT_LEN + 1);
    }
}

/* Seed selection: uniformly random over the pool. */
static const char *select_seed(void)
{
    return num_seeds ? seeds[rand() % num_seeds] : NULL;
}

/* Mutation: copy the seed, then replace one random position with a random
 * grammar character. Changing one character at a time keeps whatever
 * progress the seed made on the other characters. */
static void mutate(const char *seed, char *out)
{
    memcpy(out, seed, INPUT_LEN + 1);
    out[rand() % INPUT_LEN] = gen_char();
}

/* ---------- coverage ---------- */
static unsigned char global_cov[MAX_EDGES];   /* every edge seen so far */
static unsigned char run_cov[MAX_EDGES];
static int total_edges_seen;

/* Read this run's coverage file and merge it into the global map.
 * Returns 1 if the run hit at least one edge never seen before. */
static int coverage_increased(void)
{
    FILE *f = fopen(COV_FILE, "rb");
    if (!f)
        return 0;                              /* no coverage written */
    size_t n = fread(run_cov, 1, MAX_EDGES, f);
    fclose(f);

    int new_edges = 0;
    for (size_t i = 0; i < n; i++) {
        if (run_cov[i] && !global_cov[i]) {
            global_cov[i] = 1;
            new_edges++;
        }
    }
    total_edges_seen += new_edges;
    return new_edges > 0;
}

/* ---------- reporting ---------- */
static void report_crash(const char *input, int sig, unsigned long long count,
                         double secs, unsigned int seed, const char *target)
{
    printf("\n=== CRASH FOUND ===\n");
    printf("Signal: %d (%s)\n", sig, strsignal(sig));
    printf("Crash-inducing test case (hex):");
    for (int i = 0; i < INPUT_LEN; i++)
        printf(" %02x", (unsigned char)input[i]);
    printf("\n");
    printf("Crash-inducing test case (text): %s\n", input);
    printf("Replay with: %s '%s'\n", target, input);
    printf("Number of test cases: %llu\n", count);
    printf("Wall clock time: %.0f seconds\n", secs);
    printf("Seeds in pool: %d, edges covered: %d\n", num_seeds, total_edges_seen);
    printf("RNG seed: %u\n", seed);
    fflush(stdout);

    FILE *f = fopen("crash.bin", "wb");
    if (f) {
        fwrite(input, 1, INPUT_LEN, f);
        fclose(f);
        printf("Raw input saved to crash.bin\n");
    }
}

int main(int argc, char **argv)
{
    const char *target = argc > 1 ? argv[1] : "./targetCov";
    unsigned int seed = argc > 2 ? (unsigned int)strtoul(argv[2], NULL, 10)
                                 : (unsigned int)(time(NULL) ^ (getpid() << 16));

    if (access(target, X_OK) != 0) {
        fprintf(stderr, "error: cannot execute %s\n", target);
        return 2;
    }

    srand(seed);
    setenv("COV_FILE", COV_FILE, 1);           /* tell the SUT where to write */
    printf("Fuzzing %s with RNG seed %u\n", target, seed);
    fflush(stdout);

    char input[INPUT_LEN + 1];
    char cmd[4096];
    unsigned long long count = 0;
    time_t start = time(NULL);

    for (;;) {
        /* 1-2. Seed selection + mutation (or generation if pool is empty) */
        const char *s = select_seed();
        if (s)
            mutate(s, input);
        else
            gen_input(input);

        /* 3. SUT execution. Remove the old coverage file first so a run that
         *    writes nothing can't be mistaken for the previous run's coverage. */
        unlink(COV_FILE);
        snprintf(cmd, sizeof cmd, "%s '%s' > /dev/null 2>&1", target, input);
        int status = system(cmd);
        count++;

        /* 4. Execution analysis */
        if (status == -1) {
            perror("system");
            return 2;
        }
        int sig = 0;
        if (WIFSIGNALED(status))
            sig = WTERMSIG(status);
        else if (WIFEXITED(status) && WEXITSTATUS(status) > 128)
            sig = WEXITSTATUS(status) - 128;

        if (sig == SIGINT || sig == SIGQUIT) {
            fprintf(stderr, "\ninterrupted after %llu test cases\n", count);
            return 130;
        }
        if (sig != 0) {
            report_crash(input, sig, count, difftime(time(NULL), start), seed, target);
            return 0;
        }

        if (coverage_increased()) {
            add_seed(input);
            fprintf(stderr, "[%.0fs] test %llu: new coverage from '%s' "
                    "(%d edges, %d seeds)\n", difftime(time(NULL), start),
                    count, input, total_edges_seen, num_seeds);
        }

        if (count % PROGRESS_EVERY == 0) {
            double secs = difftime(time(NULL), start);
            fprintf(stderr, "[%.0fs] %llu test cases, %d seeds, %d edges, %.0f/s\n",
                    secs, count, num_seeds, total_edges_seen,
                    secs > 0 ? count / secs : 0.0);
        }
    }
}