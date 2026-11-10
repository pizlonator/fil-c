#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/capability.h>

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

    /* The runtime is multi-threaded, so a user namespace is refused: with an error,
       not by stopping the program. */
    CHECK(unshare(CLONE_NEWUSER) < 0);
    CHECK(errno == EINVAL || errno == EPERM);

    printf("linuxcalls ok\n");
    return 0;
}
