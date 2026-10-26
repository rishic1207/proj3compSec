/*
 * CS 4264 Project 3 - Objective 1: black-box random fuzzer
 *
 * Generates completely random test cases (every byte value except 0x00 and
 * the single quote 0x27), runs the SUT on each one with system(), and stops
 * at the first crash. Reports the crashing input, the number of test cases,
 * and the wall-clock time.
 *
 * Usage: ./fuzzer [target_path] [rng_seed]
 *   target_path defaults to ./target
 *   rng_seed    defaults to time(NULL) ^ pid, so concurrent trials differ
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LEN      8            /* longest input length we step up to       */
#define BUDGET_MULT  3ULL         /* tries per length = 3 * 254^len (~95% hit) */
#define BUDGET_CAP   2000000000ULL
#define NUM_BYTES    254ULL       /* 255 nonzero byte values minus the quote   */
#define PROGRESS_EVERY 100000ULL

/* Return a random byte in 0x01..0xFF, never 0x27 ('). 254 possible values. */
static unsigned char random_byte(void)
{
    unsigned char b = (unsigned char)(rand() % NUM_BYTES + 1); /* 1..254 */
    if (b >= 0x27)
        b++;                                                    /* skip ' */
    return b;
}

/* How many random test cases to try at a given length before moving on.
 * No tracking of past inputs is needed: we just sample with replacement. */
static unsigned long long budget_for(int len)
{
    unsigned long long n = BUDGET_MULT;
    for (int i = 0; i < len; i++) {
        n *= NUM_BYTES;
        if (n > BUDGET_CAP)
            return BUDGET_CAP;
    }
    return n;
}

static void report_crash(const unsigned char *input, int len, int sig,
                         unsigned long long count, double secs,
                         unsigned int seed, const char *target)
{
    printf("\n=== CRASH FOUND ===\n");
    printf("Signal: %d (%s)\n", sig, strsignal(sig));

    printf("Crash-inducing test case (hex):");
    for (int i = 0; i < len; i++)
        printf(" %02x", input[i]);
    printf("\n");

    printf("Crash-inducing test case (text): ");
    for (int i = 0; i < len; i++)
        putchar(input[i] >= 0x20 && input[i] < 0x7f ? input[i] : '.');
    printf("\n");

    printf("Replay with: %s $'", target);
    for (int i = 0; i < len; i++)
        printf("\\x%02x", input[i]);
    printf("'\n");

    printf("Number of test cases: %llu\n", count);
    printf("Wall clock time: %.0f seconds\n", secs);
    printf("RNG seed: %u\n", seed);
    fflush(stdout);

    /* Also save the raw bytes so the input can be replayed exactly. */
    FILE *f = fopen("crash.bin", "wb");
    if (f) {
        fwrite(input, 1, len, f);
        fclose(f);
        printf("Raw input saved to crash.bin\n");
    }
}

int main(int argc, char **argv)
{
    const char *target = argc > 1 ? argv[1] : "./target";
    unsigned int seed = argc > 2 ? (unsigned int)strtoul(argv[2], NULL, 10)
                                 : (unsigned int)(time(NULL) ^ (getpid() << 16));

    if (access(target, X_OK) != 0) {
        fprintf(stderr, "error: cannot execute %s\n", target);
        return 2;
    }

    srand(seed);
    printf("Fuzzing %s with RNG seed %u\n", target, seed);
    fflush(stdout);

    unsigned char input[MAX_LEN + 1];
    /* Worst case: path + quotes + redirect + up to 255-byte input */
    char cmd[4096];
    unsigned long long count = 0;
    time_t start = time(NULL);

    for (;;) { /* cycle through lengths forever until something crashes */
        for (int len = 1; len <= MAX_LEN; len++) {
            unsigned long long budget = budget_for(len);
            for (unsigned long long i = 0; i < budget; i++) {
                /* 1. Test case generation */
                for (int j = 0; j < len; j++)
                    input[j] = random_byte();
                input[len] = '\0';

                /* 2. SUT execution: single quotes make sh pass every byte
                 *    except ' literally, which is why ' is excluded. */
                snprintf(cmd, sizeof cmd, "%s '%s' > /dev/null 2>&1",
                         target, (char *)input);
                int status = system(cmd);
                count++;

                /* 3. Execution analysis */
                if (status == -1) {
                    perror("system");
                    return 2;
                }
                int sig = 0;
                if (WIFSIGNALED(status)) {
                    sig = WTERMSIG(status);              /* target killed directly */
                } else if (WIFEXITED(status) && WEXITSTATUS(status) > 128) {
                    sig = WEXITSTATUS(status) - 128;     /* sh reported 128+signal */
                }

                /* Ctrl-C while fuzzing reaches the child too; don't call it a crash. */
                if (sig == SIGINT || sig == SIGQUIT) {
                    fprintf(stderr, "\ninterrupted after %llu test cases\n", count);
                    return 130;
                }

                if (sig != 0) {
                    double secs = difftime(time(NULL), start);
                    report_crash(input, len, sig, count, secs, seed, target);
                    return 0;
                }

                if (count % PROGRESS_EVERY == 0) {
                    double secs = difftime(time(NULL), start);
                    fprintf(stderr, "[%.0fs] %llu test cases, len %d, %.0f/s\n",
                            secs, count, len, secs > 0 ? count / secs : 0.0);
                }
            }
        }
    }
}