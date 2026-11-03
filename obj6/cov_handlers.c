/*
 * CS 4264 Project 3 - Objective 5: SanitizerCoverage edge hit-count handlers
 *
 * Linked into the SUT, which is compiled with
 *     clang -fsanitize-coverage=inline-8bit-counters
 * This file itself must be compiled WITHOUT that flag.
 *
 * With inline-8bit-counters, the compiler gives every edge a 1-byte counter
 * and inserts an inline "counter++" on that edge -- no function call per
 * edge, unlike trace-pc-guard. Our only job is to learn where the counters
 * live (the init callback below) and dump them when the SUT exits.
 *
 * Coverage file: one byte per edge = how many times that edge ran during
 * this execution (the counter is 8 bits, so it wraps after 255).
 * Path: $COV_FILE if set, otherwise ./cov.bin
 */
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_MODULES 16

static uint8_t *counter_start[MAX_MODULES];
static uint8_t *counter_end[MAX_MODULES];
static int      num_modules;

/* Called once per instrumented module at startup with its counter array. */
void __sanitizer_cov_8bit_counters_init(char *start, char *end)
{
    if (start == end || num_modules >= MAX_MODULES)
        return;
    for (int i = 0; i < num_modules; i++)
        if (counter_start[i] == (uint8_t *)start)
            return;                          /* already registered */
    counter_start[num_modules] = (uint8_t *)start;
    counter_end[num_modules]   = (uint8_t *)end;
    num_modules++;
}

static void write_all(int fd, const uint8_t *buf, size_t len)
{
    while (len > 0) {
        ssize_t w = write(fd, buf, len);
        if (w <= 0)
            return;
        buf += w;
        len -= (size_t)w;
    }
}

/* Write every module's counters back to back when the SUT exits normally.
 * A crashing run never reaches this, which is fine: a crash ends fuzzing. */
__attribute__((destructor))
static void dump_coverage(void)
{
    const char *path = getenv("COV_FILE");
    if (!path)
        path = "cov.bin";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return;
    for (int i = 0; i < num_modules; i++)
        write_all(fd, counter_start[i], (size_t)(counter_end[i] - counter_start[i]));
    close(fd);
}