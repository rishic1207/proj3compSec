#ifndef FUZZ_RUNNER_H
#define FUZZ_RUNNER_H
#include <stddef.h>
typedef enum { NORMAL, CRASH, TIMEOUT, EXEC_FAIL } outcome_kind;
typedef struct { outcome_kind kind; int exit_code; int signal_number; double elapsed_ms; } outcome_t;
int run_target(const char *path, const char *arg, int timeout_ms, outcome_t *out);
int run_target_pdf(const char *path, const char *pdf_file, int timeout_ms, outcome_t *out);
#endif
