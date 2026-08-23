/* Exercises the inotify syscalls through the raw syscall(2) dispatch, the way
   Qt's file-system watcher invokes them (it bypasses the libc wrappers). */

#include <sys/inotify.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdfil.h>

int main()
{
    int fd = syscall(SYS_inotify_init1, IN_CLOEXEC);
    ZASSERT(fd > 2);

    char dir[] = "/tmp/filc_inotify_XXXXXX";
    ZASSERT(mkdtemp(dir));

    int wd = syscall(SYS_inotify_add_watch, fd, dir, IN_CREATE);
    ZASSERT(wd >= 0);

    /* Create a file in the watched directory to generate an event. */
    char path[256];
    snprintf(path, sizeof(path), "%s/hello", dir);
    int f = open(path, O_CREAT | O_WRONLY, 0600);
    ZASSERT(f >= 0);
    close(f);

    /* Read the event back and check it names the file we just made. */
    char buf[sizeof(struct inotify_event) + 256];
    ssize_t n = read(fd, buf, sizeof(buf));
    ZASSERT(n >= (ssize_t)sizeof(struct inotify_event));
    struct inotify_event* ev = (struct inotify_event*)buf;
    ZASSERT(ev->mask & IN_CREATE);
    ZASSERT(!strcmp(ev->name, "hello"));

    ZASSERT(syscall(SYS_inotify_rm_watch, fd, wd) == 0);
    close(fd);

    unlink(path);
    rmdir(dir);
    return 0;
}
