/* Smoke-tests the new mount API syscalls via syscall(2).  Each of these must route to a
   pizlonated wrapper that validates its pointer arguments and then performs the real syscall.
   The kernel then rejects the call gracefully.  If dispatch was broken, we would panic with
   "unsupported syscall" instead of seeing a kernel errno.  We don't check which errno we get,
   since that depends on the kernel version, seccomp policy, and our capabilities, but it must
   be some errno.

   Like util-linux's mountutils.h, we define the mount API constants and structs locally so
   that this test works with any libc and any kernel headers. */

#define _GNU_SOURCE

#include <errno.h>
#include <stdint.h>
#include <stdfil.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_open_tree
#define SYS_open_tree 428
#endif
#ifndef SYS_move_mount
#define SYS_move_mount 429
#endif
#ifndef SYS_fsopen
#define SYS_fsopen 430
#endif
#ifndef SYS_fsconfig
#define SYS_fsconfig 431
#endif
#ifndef SYS_fsmount
#define SYS_fsmount 432
#endif
#ifndef SYS_fspick
#define SYS_fspick 433
#endif
#ifndef SYS_mount_setattr
#define SYS_mount_setattr 442
#endif

#ifndef AT_FDCWD
#define AT_FDCWD -100 /* Special value used to indicate the current working directory */
#endif
#ifndef OPEN_TREE_CLONE
#define OPEN_TREE_CLONE 1
#endif
#ifndef FSPICK_CLOEXEC
#define FSPICK_CLOEXEC 0x00000001
#endif
#ifndef FSCONFIG_SET_FLAG
#define FSCONFIG_SET_FLAG 0
#endif

/* Local copy of struct mount_attr (see mountutils.h). */
struct mount_attr {
    uint64_t attr_set;
    uint64_t attr_clr;
    uint64_t propagation;
    uint64_t userns_fd;
};

static void check(const char* what, int rc)
{
    printf("%s rc %d errno %d\n", what, rc, errno);
    ZASSERT(rc == -1);
    ZASSERT(errno != 0);
}

int main(void)
{
    struct mount_attr attr;
    memset(&attr, 0, sizeof(attr));

    /* The wrappers for these syscalls return int, so we cast the syscall(2) result to int;
       failure reads as -1, just like it does for any other libc caller. */

    errno = 0;
    check("fsopen", (int)syscall(SYS_fsopen, "definitely-not-a-real-filc-fs", 0));

    errno = 0;
    check("open_tree", (int)syscall(SYS_open_tree, AT_FDCWD,
                                    "/nonexistent-filc-mountapi-path", OPEN_TREE_CLONE));

    errno = 0;
    check("move_mount", (int)syscall(SYS_move_mount, AT_FDCWD, "/nonexistent-filc-a",
                                     AT_FDCWD, "/nonexistent-filc-b", 0));

    errno = 0;
    /* Weirdly, this returns 0 in some containers. */
    syscall(SYS_mount_setattr, AT_FDCWD,
            "/nonexistent-filc-mountapi", 0, &attr, sizeof(attr));

    errno = 0;
    check("fsconfig", (int)syscall(SYS_fsconfig, -1, FSCONFIG_SET_FLAG,
                                   "definitely-not-a-real-filc-key", NULL, 0));

    errno = 0;
    check("fsmount", (int)syscall(SYS_fsmount, -1, 0, 0));

    errno = 0;
    check("fspick", (int)syscall(SYS_fspick, AT_FDCWD, "/nonexistent-filc-mountapi",
                                 FSPICK_CLOEXEC));

    printf("mountapi ok\n");
    return 0;
}
