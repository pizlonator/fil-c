#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

/* clone(2) as a fork that also creates the child in new namespaces. The parent stays in its own
   namespaces, so it keeps its collector, and the child is the first process of its own PID
   namespace and gets a collector too. Clones that share memory or make threads are refused. */

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

static int count_threads(void)
{
    DIR* dir = opendir("/proc/self/task");
    int count = 0;
    struct dirent* entry;
    if (!dir)
        return -1;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] != '.')
            count++;
    }
    closedir(dir);
    return count;
}

int main(void)
{
    /* What a fork cannot be. */
    errno = 0;
    CHECK(syscall(SYS_clone, SIGCHLD | CLONE_VM, 0, 0, 0, 0) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(syscall(SYS_clone, SIGCHLD | CLONE_THREAD, 0, 0, 0, 0) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(syscall(SYS_clone, SIGCHLD, 1, 0, 0, 0) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(syscall(SYS_clone, CLONE_NEWUSER | CLONE_NEWPID, 0, 0, 0, 0) == -1 && errno == EINVAL);

    long child = syscall(SYS_clone, SIGCHLD | CLONE_NEWUSER | CLONE_NEWPID, 0, 0, 0, 0);
    CHECK(child >= 0);
    if (!child) {
        if (getpid() != 1)
            _exit(4);
        churn();
        zgc_wait(zgc_request_fresh());
        if (count_threads() < 2)
            _exit(5);
        _exit(0);
    }

    /* The parent did not enter the namespace: it can make threads and has its collector. */
    churn();
    zgc_wait(zgc_request_fresh());
    CHECK(count_threads() >= 2);

    int status;
    CHECK(waitpid((pid_t)child, &status, 0) == (pid_t)child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
    printf("clonens ok\n");
    return 0;
}
