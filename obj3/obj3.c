/*
 * CS 4264 Project 3 - Objective 3: grammar-based fuzzer
 *
 * Same as Objective 1, except test cases are generated from the SUT's grammar
 * so every input is syntactically valid:
 *     <input> ::= <char><char><char>
 *     <char>  ::= any lowercase letter | any uppercase letter | any digit
 * (regex: [0-9a-zA-Z]{3}). Runs the SUT with system() and stops at the first
 * crash, reporting the crashing input, test case count, and wall-clock time.
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

#define INPUT_LEN      3          /* <input> ::= <char><char><char>       */
#define PROGRESS_EVERY 10000ULL

/* <char> ::= lowercase | uppercase | digit  (62 choices, all equally likely) */
static const char CHARSET[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
#define CHARSET_SIZE (sizeof(CHARSET) - 1)   /* 62 */

static unsigned char gen_char(void)
{
    return (unsigned char)CHARSET[rand() % CHARSET_SIZE];
}

/* <input>: expand the grammar's top rule into three <char>s. Inputs are
 * sampled with replacement, so no history of past inputs is kept. */
static void gen_input(unsigned char *buf)
{
    for (int i = 0; i < INPUT_LEN; i++)
        buf[i] = gen_char();
    buf[INPUT_LEN] = '\0';
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

    unsigned char input[INPUT_LEN + 1];
    char cmd[4096];
    unsigned long long count = 0;
    time_t start = time(NULL);
    const int len = INPUT_LEN;

    for (;;) {
                /* 1. Test case generation from the grammar */
                gen_input(input);

                /* 2. SUT execution: single quotes make sh pass every byte
                 *    literally. */
                snprintf(cmd, sizeof cmd, "%s '%s' > /dev/null 2>&1",
                         target, (char *)input);  /* only [0-9a-zA-Z]: always safe in quotes */
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
                    fprintf(stderr, "[%.0fs] %llu test cases, %.0f/s\n",
                            secs, count, secs > 0 ? count / secs : 0.0);
                }
    }
}