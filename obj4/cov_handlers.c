/*
 * CS 4264 Project 3 - Objective 4: SanitizerCoverage edge-coverage handlers
 *
 * Linked into the SUT, which is compiled with
 *     clang -fsanitize-coverage=trace-pc-guard
 * This file itself must be compiled WITHOUT that flag, or the callbacks
 * would instrument themselves.
 *
 * Each instrumented edge gets a guard (a uint32_t). At startup we number the
 * guards 1..N. When an edge runs, we mark it in a byte map and zero its guard
 * so later hits of the same edge cost almost nothing (edge coverage only
 * needs "was it hit", not "how often"). When the SUT exits normally, the map
 * (one byte per edge, 1 = covered) is written to the coverage file, which the
 * fuzzer then reads.
 *
 * Coverage file: $COV_FILE if set, otherwise ./cov.bin
 */
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_EDGES 65536

static uint8_t  edge_map[MAX_EDGES];
static uint32_t num_edges;

/* Called once per instrumented module at startup with its range of guards. */
void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop)
{
    if (start == stop || *start)
        return;                              /* already initialized */
    for (uint32_t *g = start; g < stop; g++) {
        if (num_edges + 1 < MAX_EDGES)
            *g = ++num_edges;                /* guard ids are 1..N */
        else
            *g = 0;                          /* too many edges: don't track */
    }
}

/* Called on every instrumented edge. */
void __sanitizer_cov_trace_pc_guard(uint32_t *guard)
{
    if (!*guard)
        return;                              /* already recorded */
    edge_map[*guard] = 1;
    *guard = 0;
}

/* Write the map when the SUT exits normally (return from main or exit()).
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
    size_t len = (size_t)num_edges + 1, off = 0;
    while (off < len) {
        ssize_t w = write(fd, edge_map + off, len - off);
        if (w <= 0)
            break;
        off += (size_t)w;
    }
    close(fd);
}