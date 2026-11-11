#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdfil.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/capability.h>
#include <sys/wait.h>

#define CHECK(x) do { \
    if (!(x)) { \
        printf("FAIL %s:%d: %s (errno = %d)\n", __FILE__, __LINE__, #x, errno); \
        exit(1); \
    } \
} while (0)

int main(void)
{
    int fd = open("/", O_RDONLY | O_DIRECTORY);
    CHECK(fd >= 0);
    CHECK(!syncfs(fd));
    CHECK(!close(fd));

    /* A pidfd: the signal 0 probes whether the process exists. */
    int pidfd = (int)syscall(SYS_pidfd_open, getpid(), 0);
    CHECK(pidfd >= 0);
    CHECK(!syscall(SYS_pidfd_send_signal, pidfd, 0, NULL, 0));
    CHECK(!close(pidfd));

    /* The capability bounding set: a read answers 0 or 1, and capget() works through syscall(). */
    CHECK(prctl(PR_CAPBSET_READ, CAP_CHOWN) >= 0);
    struct __user_cap_header_struct header = { .version = _LINUX_CAPABILITY_VERSION_3 };
    struct __user_cap_data_struct data[2];
    CHECK(!syscall(SYS_capget, &header, data));

    /* A user namespace needs a process with one thread. The runtime stops its own threads for
       the call, and the program runs on the thread that the process started with. Do it in a
       child, so that the parent keeps its identity. */
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        if (unshare(CLONE_NEWUSER)) {
            printf("FAIL %s:%d: unshare: errno = %d\n", __FILE__, __LINE__, errno);
            _exit(1);
        }
        /* An identity without a mapping is the overflow uid. */
        if (getuid() != 65534)
            _exit(2);
        /* The runtime must still work: allocate enough to need a collection. */
        for (int i = 0; i < 100000; ++i) {
            char* p = malloc(1000);
            if (!p)
                _exit(3);
            p[0] = 1;
        }
        zgc_wait(zgc_request_fresh());
        _exit(0);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);

    printf("linuxcalls ok\n");
    return 0;
}
