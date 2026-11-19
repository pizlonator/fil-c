#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* After unshare(CLONE_NEWPID) the kernel does not let a process create threads, but it does let it
   fork. The runtime has to cope: the process that unshared carries on without its collector, and a
   child of it, which is a member of the new namespace, gets a collector of its own. */

#define CHECK(exp) do { \
    if (!(exp)) { \
        printf("FAIL %s:%d: %s (errno = %d)\n", __FILE__, __LINE__, #exp, errno); \
        exit(1); \
    } \
} while (0)

static void churn(void)
{
    for (int i = 0; i < 100000; ++i) {
        char* p = malloc(1000);
        if (!p)
            _exit(3);
        p[0] = 1;
    }
}

int main(void)
{
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        if (unshare(CLONE_NEWUSER | CLONE_NEWPID)) {
            printf("FAIL %s:%d: unshare: errno = %d\n", __FILE__, __LINE__, errno);
            _exit(1);
        }
        /* This process has no collector now. It must neither fail nor wait for one. */
        churn();
        zgc_wait(zgc_request_fresh());
        pid_t grandchild = fork();
        if (grandchild < 0) {
            printf("FAIL %s:%d: fork: errno = %d\n", __FILE__, __LINE__, errno);
            _exit(1);
        }
        if (!grandchild) {
            /* The first process of the namespace, with a collector of its own. */
            if (getpid() != 1)
                _exit(4);
            churn();
            zgc_wait(zgc_request_fresh());
            _exit(0);
        }
        int status;
        if (waitpid(grandchild, &status, 0) != grandchild || !WIFEXITED(status))
            _exit(5);
        _exit(WEXITSTATUS(status));
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
    printf("newpid ok\n");
    return 0;
}
