#define _POSIX_C_SOURCE 200809L
#include "runner.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static double ms_since(struct timespec begin) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - begin.tv_sec) * 1000.0 + (now.tv_nsec - begin.tv_nsec) / 1e6;
}
/* path is executable filename; arg is one NUL-terminated command-line argument.
   Binary PDF mode should pass a filename here, never the PDF data itself. */
static int run_impl(const char *path, const char *arg, int pdf_mode, int timeout_ms, outcome_t *out) {
    if (!path || !arg || !out || timeout_ms <= 0) { errno = EINVAL; return -1; }
    memset(out, 0, sizeof *out);
    int errpipe[2];
    if (pipe(errpipe) != 0) return -1;
    fcntl(errpipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(errpipe[1], F_SETFD, FD_CLOEXEC);
    struct timespec begin;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    pid_t pid = fork();
    if (pid < 0) { close(errpipe[0]); close(errpipe[1]); return -1; }
    if (pid == 0) {
        close(errpipe[0]);
        /* Prevent large target stdout/stderr from dominating fuzz throughput. */
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) { dup2(nullfd, STDOUT_FILENO); dup2(nullfd, STDERR_FILENO); close(nullfd); }
        char *const text_args[] = {(char *)path, (char *)arg, NULL};
        char *const pdf_args[] = {(char *)path, "-q", (char *)arg, "-", NULL};
        execv(path, pdf_mode ? pdf_args : text_args);
        int saved = errno;
        (void)write(errpipe[1], &saved, sizeof saved);
        _exit(127);
    }
    close(errpipe[1]);
    int status = 0;
    int timed_out = 0;
    for (;;) {
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) break;
        if (waited < 0) {
            if (errno == EINTR) continue;
            close(errpipe[0]);
            return -1;
        }
        if (ms_since(begin) >= timeout_ms) {
            /* Recheck before killing to reduce the completion/timeout race. */
            waited = waitpid(pid, &status, WNOHANG);
            if (waited == pid) break;
            if (waited < 0 && errno != EINTR) { close(errpipe[0]); return -1; }
            if (waited == 0) {
                if (kill(pid, SIGKILL) == 0) timed_out = 1;
                while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
                break;
            }
        }
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000000L};
        nanosleep(&pause, NULL);
    }
    out->elapsed_ms = ms_since(begin);
    int exec_errno = 0;
    ssize_t got;
    do { got = read(errpipe[0], &exec_errno, sizeof exec_errno); } while (got < 0 && errno == EINTR);
    close(errpipe[0]);
    if (got == sizeof exec_errno) { out->kind = EXEC_FAIL; out->exit_code = exec_errno; }
    else if (timed_out) { out->kind = TIMEOUT; out->signal_number = SIGKILL; }
    else if (WIFSIGNALED(status)) { out->kind = CRASH; out->signal_number = WTERMSIG(status); }
    else if (WIFEXITED(status)) { out->kind = NORMAL; out->exit_code = WEXITSTATUS(status); }
    else { errno = ECHILD; return -1; }
    return 0;
}

int run_target(const char *p, const char *a, int t, outcome_t *o) {return run_impl(p,a,0,t,o);}
int run_target_pdf(const char *p, const char *a, int t, outcome_t *o) {return run_impl(p,a,1,t,o);}
