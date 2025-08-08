#define _GNU_SOURCE

#include <pizlonated_syscalls.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <sys/time.h>

struct foo {
    char* a;
    int b;
    int c;
    int d;
};

int main(int argc, char** argv)
{
    int fd = zsys_open("filc/tests/fileio/test.txt", 0);
    ZASSERT(fd > 2);

    char buf[100];
    __builtin_memset(buf, 0, sizeof(buf));
    int result = zsys_read(fd, buf, sizeof(buf));
    ZASSERT(result == 14);
    zprintf("read the zsys way: %s", buf);
    ZASSERT(!zsys_close(fd));

    fd = open("filc/tests/fileio/test.txt", O_RDONLY);
    ZASSERT(fd > 2);
    ZASSERT(!posix_fadvise(fd, 0, 1, POSIX_FADV_NORMAL));
    __builtin_memset(buf, 0, sizeof(buf));
    result = read(fd, buf, sizeof(buf));
    ZASSERT(result == 14);
    zprintf("read with read way: %s", buf);
    ZASSERT(!close(fd));

    fd = openat(AT_FDCWD, "filc/tests/fileio/test.txt", O_RDONLY);
    ZASSERT(fd > 2);
    __builtin_memset(buf, 0, sizeof(buf));
    result = read(fd, buf, sizeof(buf));
    ZASSERT(result == 14);
    zprintf("read with read way: %s", buf);
    ZASSERT(!close(fd));

    ZASSERT(!errno);
    FILE* fin = fopen("filc/tests/fileio/test.txt", "r");
    ZASSERT(!errno);
    ZASSERT(fin);
    __builtin_memset(buf, 0, sizeof(buf));
    result = fread(buf, 1, sizeof(buf), fin);
    ZASSERT(result == 14);
    printf("read the fread way: %s", buf);
    fclose(fin);

    fd = open("filc/tests/fileio/test.txt", O_RDONLY);
    ZASSERT(fd > 2);
    int fd2 = fcntl(fd, F_DUPFD, 100);
    ZASSERT(fd2 >= 100);
    __builtin_memset(buf, 0, sizeof(buf));
    result = read(fd2, buf, sizeof(buf));
    ZASSERT(result == 14);
    zprintf("read with fcntl DUPFD read way: %s", buf);
    __builtin_memset(buf, 0, sizeof(buf));
    result = read(fd, buf, sizeof(buf));
    ZASSERT(!result);
    ZASSERT(!close(fd));

    struct flock file_lock;
    memset(&file_lock, 0, sizeof(flock));
    file_lock.l_type = F_RDLCK;
    int res = fcntl(fd2, F_GETLK, &file_lock);
    ZASSERT(!res);
    ZASSERT(file_lock.l_type == F_UNLCK);
    ZASSERT(!file_lock.l_whence);
    ZASSERT(!file_lock.l_start);
    ZASSERT(!file_lock.l_len);
    ZASSERT(!file_lock.l_pid);
    ZASSERT(!close(fd2));
    
    fd = open("filc/tests/fileio/test.txt", O_RDONLY);
    ZASSERT(fd > 2);
    file_lock.l_type = F_WRLCK;
    res = fcntl(fd, F_GETLK, &file_lock);
    ZASSERT(!res);
    ZASSERT(file_lock.l_type == F_UNLCK);
    ZASSERT(!file_lock.l_whence);
    ZASSERT(!file_lock.l_start);
    ZASSERT(!file_lock.l_len);
    ZASSERT(!file_lock.l_pid);

    res = fcntl(fd, F_GETFD);
    ZASSERT(!res);

    res = fcntl(fd, F_SETFD, FD_CLOEXEC);
    ZASSERT(!res);

    res = fcntl(fd, F_GETFD);
    ZASSERT(res == FD_CLOEXEC);

    ZASSERT(!flock(fd, LOCK_EX));
    ZASSERT(!flock(fd, LOCK_UN));
    ZASSERT(!flock(fd, LOCK_SH));
    ZASSERT(!flock(fd, LOCK_UN));

    ZASSERT(!isatty(fd));
    ZASSERT(!close(fd));

    int fds[2];
    ZASSERT(!pipe(fds));
    int data = 666;
    ZASSERT(!ioctl(fds[0], FIONREAD, &data));
    ZASSERT(!data);
    ZASSERT(write(fds[1], "hello", strlen("hello") + 1) == strlen("hello") + 1);
    ZASSERT(!ioctl(fds[0], FIONREAD, &data));
    ZASSERT(data == strlen("hello") + 1);

    ZASSERT(ioctl(666, FIONREAD, &data) == -1);
    ZASSERT(errno == EBADF);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fds[0], &readfds);
    ZASSERT(select(fds[0] + 1, &readfds, NULL, NULL, NULL) == 1);
    ZASSERT(FD_ISSET(fds[0], &readfds));
    ZASSERT(pselect(fds[0] + 1, &readfds, NULL, NULL, NULL, NULL) == 1);
    ZASSERT(FD_ISSET(fds[0], &readfds));

    struct pollfd pollfds[1];
    pollfds[0].fd = fds[0];
    pollfds[0].events = POLLIN;
    ZASSERT(poll(pollfds, 1, -1) == 1);
    ZASSERT(pollfds[0].revents == POLLIN);

    bzero(pollfds, sizeof(pollfds));
    pollfds[0].fd = fds[0];
    pollfds[0].events = POLLIN;
    ZASSERT(ppoll(pollfds, 1, NULL, NULL) == 1);
    ZASSERT(pollfds[0].revents == POLLIN);

    int epfd = epoll_create1(0);
    ZASSERT(epfd > 2);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fds[0];
    ZASSERT(!epoll_ctl(epfd, EPOLL_CTL_ADD, fds[0], &ev));
    memset(&ev, 0, sizeof(ev));
    ZASSERT(epoll_wait(epfd, &ev, 1, -1) == 1);
    ZASSERT(ev.events == EPOLLIN);
    ZASSERT(ev.data.fd == fds[0]);
    int epfd2 = dup(epfd);
    ZASSERT(epfd2 > 2);
    memset(&ev, 0, sizeof(ev));
    ZASSERT(epoll_wait(epfd2, &ev, 1, -1) == 1);
    ZASSERT(ev.events == EPOLLIN);
    ZASSERT(ev.data.fd == fds[0]);
    ZASSERT(!close(epfd));
    memset(&ev, 0, sizeof(ev));
    ZASSERT(epoll_wait(epfd2, &ev, 1, -1) == 1);
    ZASSERT(ev.events == EPOLLIN);
    ZASSERT(ev.data.fd == fds[0]);
    memset(&ev, 0, sizeof(ev));
    ZASSERT(epoll_pwait(epfd2, &ev, 1, -1, NULL) == 1);
    ZASSERT(ev.events == EPOLLIN);
    ZASSERT(ev.data.fd == fds[0]);
#ifdef __USE_GNU
    memset(&ev, 0, sizeof(ev));
    ZASSERT(epoll_pwait2(epfd2, &ev, 1, NULL, NULL) == 1);
    ZASSERT(ev.events == EPOLLIN);
    ZASSERT(ev.data.fd == fds[0]);
#endif
    ZASSERT(!close(epfd2));
    
    ZASSERT(read(fds[0], buf, strlen("hello") + 1) == strlen("hello") + 1);
    ZASSERT(!strcmp(buf, "hello"));

    int dupfd = dup(fds[0]);
    ZASSERT(dupfd > 2);
    ZASSERT(write(fds[1], "world", strlen("world") + 1) == strlen("world") + 1);
    ZASSERT(read(dupfd, buf, strlen("world") + 1) == strlen("world") + 1);
    ZASSERT(!strcmp(buf, "world"));
    
    dupfd = dup2(fds[0], 100);
    ZASSERT(dupfd == 100);
    ZASSERT(write(fds[1], "witam", strlen("witam") + 1) == strlen("witam") + 1);
    ZASSERT(read(dupfd, buf, strlen("witam") + 1) == strlen("witam") + 1);
    ZASSERT(!strcmp(buf, "witam"));

    DIR* dir = opendir("filc/tests/fileio");
    ZASSERT(dir);
    bool saw_fileio_c = false;
    bool saw_fileio = false;
    bool saw_manifest = false;
    bool saw_test_txt = false;
    for (;;) {
        errno = 0;
        struct dirent* de = readdir(dir);
        if (!de) {
            ZASSERT(!errno);
            break;
        }
        if (!strcmp(de->d_name, "fileio.c"))
            saw_fileio_c = true;
        if (!strcmp(de->d_name, "fileio"))
            saw_fileio = true;
        if (!strcmp(de->d_name, "manifest"))
            saw_manifest = true;
        if (!strcmp(de->d_name, "test.txt"))
            saw_test_txt = true;
    }
    ZASSERT(!closedir(dir));
    ZASSERT(saw_fileio_c);
    ZASSERT(!saw_fileio);
    ZASSERT(saw_manifest);
    ZASSERT(saw_test_txt);
    
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));
    ZASSERT(write(socks[1], "hello", strlen("hello") + 1) == strlen("hello") + 1);
    ZASSERT(read(socks[0], buf, strlen("hello") + 1) == strlen("hello") + 1);
    ZASSERT(!strcmp(buf, "hello"));

    ZASSERT(read(666, buf, 100) == -1);
    ZASSERT(errno == EBADF);
    errno = 0;

    fd = open("filc/test-output/fileio/piotest.txt", O_CREAT | O_RDWR, 0644);
    ZASSERT(fd > 2);
    const char* str = "This is a test. I repeat, this is a test.";
    ZASSERT(pwrite(fd, str, strlen(str), 0) == strlen(str));
    ZASSERT(!fdatasync(fd));
    ZASSERT(!fsync(fd));
    memset(buf, 0, sizeof(buf));
    ZASSERT(pread(fd, buf, 14, 10) == 14);
    ZASSERT(!strcmp(buf, "test. I repeat"));
    str = "A co to to";
    ZASSERT(pwrite(fd, str, strlen(str), 13) == strlen(str));
    memset(buf, 0, sizeof(buf));
    ZASSERT(pread(fd, buf, 100, 1) == 40);
    str = "his is a tesA co to tot, this is a test.";
    ZASSERT(!strcmp(buf, str));
    ZASSERT(!close(fd));

    struct foo f;
    f.a = "hello";
    f.b = 42;
    f.c = 666;
    f.d = 1410;
    ZASSERT(!errno);
    FILE* fout = fopen("filc/test-output/fileio/fiotest.txt", "w");
    ZASSERT(!errno);
    ZASSERT(fout);
    ZASSERT(1 == fwrite(&f, sizeof(f), 1, fout));
    ZASSERT(!fclose(fout));

    struct foo f2;
    fin = fopen("filc/test-output/fileio/fiotest.txt", "r");
    ZASSERT(fin);
    ZASSERT(1 == fread(&f2, sizeof(f2), 1, fin));
    ZASSERT(!fclose(fin));
    ZASSERT(!zhasvalidcap(f2.a));
    ZASSERT(f2.a == f.a);
    ZASSERT(f2.b == 42);
    ZASSERT(f2.c == 666);
    ZASSERT(f2.d == 1410);

    fd = open("filc/test-output/fileio/allocatetest.txt", O_CREAT | O_WRONLY | O_EXCL, 0600);
    ZASSERT(fd > 2);
    ZASSERT(!posix_fallocate(fd, 666, 42));
    struct stat s;
    ZASSERT(!fstat(fd, &s));
    ZASSERT(s.st_size == 666 + 42);

    struct timeval tv[2];
    ZASSERT(!gettimeofday(tv, NULL));
    tv[1] = tv[0];
    ZASSERT(!utimes("filc/test-output/fileio/allocatetest.txt", tv));
    ZASSERT(!lutimes("filc/test-output/fileio/allocatetest.txt", tv));
    result = futimes(fd, tv);
    if (result)
        zprintf("error = %s\n", strerror(errno));
    ZASSERT(!result);
    struct timespec ts[2];
    ZASSERT(!clock_gettime(CLOCK_REALTIME, ts));
    ZASSERT(!utimensat(AT_FDCWD, "filc/test-output/fileio/allocatetest.txt", ts, 0));
    ZASSERT(!futimens(fd, ts));

    ZASSERT(!close(fd));
    ZASSERT(!unlink("filc/test-output/fileio/allocatetest.txt"));

    ZASSERT(close(-1) == -1);
    ZASSERT(errno == EBADF);

    struct timeval tv2;
    tv2.tv_sec = 0;
    tv2.tv_usec = 0;
    ZASSERT(!select(0, NULL, NULL, NULL, &tv2));

    struct timespec ts2;
    ts2.tv_sec = 0;
    ts2.tv_nsec = 0;
    ZASSERT(!pselect(0, NULL, NULL, NULL, &ts2, NULL));

    fd = creat("filc/test-output/fileio/creattest.txt", 0600);
    ZASSERT(fd > 2);
    ZASSERT(write(fd, "hello", 5) == 5);
    ZASSERT(!close(fd));
    fd = open("filc/test-output/fileio/creattest.txt", O_RDONLY);
    ZASSERT(fd > 2);
    ZASSERT(read(fd, buf, 5) == 5);
    ZASSERT(!memcmp(buf, "hello", 5));
    ZASSERT(!close(fd));

    return 0;
}


