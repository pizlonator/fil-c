#include <signal.h>
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static __attribute__((noinline)) void trigger_oob_panic(void)
{
    int x = 0;
    int *volatile ptr = &x;
    *(ptr + 4) = 42;
    printf("UNEXPECTED_OOB_SURVIVED\n");
    fflush(stdout);
    _exit(0);
}

static void disable_core_dumps(void)
{
    struct rlimit rl;
    rl.rlim_cur = 0;
    rl.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rl);
}

static void close_stderr(void)
{
    ZASSERT(close(STDERR_FILENO) == 0);
}

static void setup_broken_pipe_stderr(void)
{
    int fds[2];
    ZASSERT(pipe(fds) == 0);
    ZASSERT(close(fds[0]) == 0);
    ZASSERT(dup2(fds[1], STDERR_FILENO) == STDERR_FILENO);
    ZASSERT(close(fds[1]) == 0);
    ZASSERT(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
}

static int exit_on_panic_enabled(void)
{
    const char *env = getenv("FILC_EXIT_ON_PANIC");
    if (!env)
        return 0;
    return !strcmp(env, "1") || !strcmp(env, "true") || !strcmp(env, "TRUE") ||
           !strcmp(env, "yes") || !strcmp(env, "YES");
}

static void assert_clean_panic_status(int status, int expect_exit_42)
{
    if (expect_exit_42) {
        ZASSERT(WIFEXITED(status));
        ZASSERT(WEXITSTATUS(status) == 42);
    } else {
        ZASSERT(WIFSIGNALED(status));
        ZASSERT(WTERMSIG(status) == SIGTRAP);
    }
}

static void run_fork_case(void (*setup_stderr)(void), int expect_exit_42)
{
    fflush(NULL);
    pid_t pid = fork();
    ZASSERT(pid >= 0);
    if (pid == 0) {
        disable_core_dumps();
        setup_stderr();
        trigger_oob_panic();
    }

    int status = 0;
    ZASSERT(waitpid(pid, &status, 0) == pid);
    assert_clean_panic_status(status, expect_exit_42);
}

static void run_reexec_case(const char *self, const char *mode)
{
    fflush(NULL);
    pid_t pid = fork();
    ZASSERT(pid >= 0);
    if (pid == 0) {
        ZASSERT(unsetenv("FILC_EXIT_ON_PANIC") == 0);
        ZASSERT(unsetenv("FILC_DUMP_SETUP") == 0);
        execl(self, self, mode, (char *)NULL);
        _exit(127);
    }

    int status = 0;
    ZASSERT(waitpid(pid, &status, 0) == pid);
    assert_clean_panic_status(status, 0);
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--closed-stderr")) {
        disable_core_dumps();
        close_stderr();
        trigger_oob_panic();
    }
    if (argc == 2 && !strcmp(argv[1], "--broken-pipe")) {
        disable_core_dumps();
        setup_broken_pipe_stderr();
        trigger_oob_panic();
    }

    int expect_exit_42 = exit_on_panic_enabled();
    run_fork_case(close_stderr, expect_exit_42);
    run_fork_case(setup_broken_pipe_stderr, expect_exit_42);

    /* Also exercise the default pas_panic() -> SIGTRAP path with
       FILC_EXIT_ON_PANIC unset even when invoked under filc/run-tests. */
    run_reexec_case(argv[0], "--closed-stderr");
    run_reexec_case(argv[0], "--broken-pipe");

    printf("panic closed stderr ok\n");
    return 0;
}
