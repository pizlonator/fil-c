#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <pizlonated_syscalls.h>
#include <pizlonated_runtime.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <grp.h>
#include <stdbool.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <filc_test_support.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <termios.h>
#include <sys/auxv.h>
#include <sys/times.h>
#include "utils.h"
#include <threads.h>
#include <stdlib.h>
#include <sys/timex.h>
#include <sys/mman.h>
#include <sys/prctl.h>

#ifndef SA_RESTORER
#define SA_RESTORER 0x4000000
#endif

static void sighandler(int sig) { }

int main(int argc, char** argv)
{
    unsigned zid;
    unsigned id;
    int res;
    bool is_root;
    
    zid = zsys_getuid();
    id = getuid();
    ZASSERT((int)zid >= 0);
    ZASSERT(zid == id);
    is_root = !id;
    
    zid = zsys_geteuid();
    id = geteuid();
    ZASSERT((int)zid >= 0);
    ZASSERT(zid == id);
    
    zid = zsys_getgid();
    id = getgid();
    ZASSERT((int)zid >= 0);
    ZASSERT(zid == id);
    
    zid = zsys_getpid();
    id = getpid();
    ZASSERT((int)zid >= 0);
    ZASSERT(zid == id);

    struct timespec ts;
    res = clock_gettime(CLOCK_REALTIME, &ts);
    ZASSERT(!res);
    ZASSERT(ts.tv_sec);
    ZASSERT(ts.tv_nsec < 1000000000llu);

    struct timeval tv;
    res = gettimeofday(&tv, NULL);
    ZASSERT(!res);
    ZASSERT(tv.tv_sec);
    ZASSERT(tv.tv_usec < 1000000llu);

    struct stat st;
    zprintf("size = %zu\n", sizeof(st));
    zprintf("offset of nlink = %zu\n", __builtin_offsetof(struct stat, st_nlink));
    zprintf("sizeof nlink = %zu\n", sizeof(st.st_nlink));
    zprintf("sizeof(nlink_t) = %zu\n", sizeof(nlink_t));
    res = stat("filc/tests/miscsyscall/testfile.txt", &st);
    ZASSERT(!res);
    ZASSERT(st.st_mode & S_IFREG);
    ZASSERT(st.st_nlink == 1);
    ZASSERT(st.st_uid >= 0);
    ZASSERT(st.st_size == 86);

    ZASSERT(zthread_self());
    ZASSERT(zthread_self() == zthread_self());
    ZASSERT(gettid() == zthread_self_id());

    ZASSERT(open("this/is/totally/not/going/to/exist", 666, 42) < 0);
    printf("the error was: %s\n", strerror(errno));

    int fd = open("filc/tests/miscsyscall/testfile.txt", O_RDONLY);
    ZASSERT(fd > 2);
    struct stat st2;
    res = fstat(fd, &st2);
    ZASSERT(!res);
    ZASSERT(st2.st_mode & S_IFREG);
    ZASSERT(st2.st_nlink == 1);
    ZASSERT(st2.st_uid == st.st_uid);
    ZASSERT(st2.st_gid == st.st_gid);
    ZASSERT(st2.st_size == 86);

    struct passwd* passwd = getpwuid(getuid());
    if (passwd) {
        ZASSERT(passwd);
        ZASSERT(strlen(passwd->pw_name));
        ZASSERT(strlen(passwd->pw_passwd));
        ZASSERT(passwd->pw_uid == getuid());
        ZASSERT(passwd->pw_gid == getgid());
        ZASSERT(strlen(passwd->pw_dir));
        ZASSERT(strlen(passwd->pw_shell));

        char* name = strdup(passwd->pw_name);
        char* passwdd = strdup(passwd->pw_passwd);
        char* gecos = strdup(passwd->pw_gecos);
        char* dir = strdup(passwd->pw_dir);
        char* shell = strdup(passwd->pw_shell);

        struct passwd* passwd2 = getpwuid(getuid());
        ZASSERT(passwd2);
        ZASSERT(!strcmp(passwd2->pw_name, name));
        ZASSERT(!strcmp(passwd2->pw_passwd, passwdd));
        ZASSERT(passwd2->pw_uid == getuid());
        ZASSERT(passwd2->pw_gid == getgid());
        ZASSERT(!strcmp(passwd2->pw_gecos, gecos));
        ZASSERT(!strcmp(passwd2->pw_dir, dir));
        ZASSERT(!strcmp(passwd2->pw_shell, shell));

        passwd = getpwnam(name);
        ZASSERT(passwd);
        ZASSERT(!strcmp(passwd->pw_name, name));
        ZASSERT(!strcmp(passwd->pw_passwd, passwdd));
        ZASSERT(passwd->pw_uid == getuid());
        ZASSERT(passwd->pw_gid == getgid());
        ZASSERT(!strcmp(passwd->pw_gecos, gecos));
        ZASSERT(!strcmp(passwd->pw_dir, dir));
        ZASSERT(!strcmp(passwd->pw_shell, shell));
    }

    ZASSERT(signal(SIGPIPE, SIG_IGN) == SIG_DFL);

    struct sigaction act;
    struct sigaction oact;
    act.sa_handler = SIG_DFL;
    sigfillset(&act.sa_mask);
    act.sa_flags = SA_NODEFER;
    ZASSERT(sigaction(SIGPIPE, &act, &oact) == 0);
    ZASSERT(oact.sa_handler == SIG_IGN);
    zprintf("oact.sa_flags = %x\n", oact.sa_flags);
    ZASSERT(oact.sa_flags == (SA_RESTART | SA_RESTORER));
#ifndef __USE_GNU
    ZASSERT(!sigismember(&oact.sa_mask, SIGPIPE));
    ZASSERT(!sigismember(&oact.sa_mask, SIGTERM));
#endif

    ZASSERT(sigaction(SIGPIPE, NULL, &oact) == 0);
    ZASSERT(oact.sa_handler == SIG_DFL);
    ZASSERT(oact.sa_flags == (SA_NODEFER | SA_RESTORER));
    ZASSERT(sigismember(&oact.sa_mask, SIGPIPE));
    ZASSERT(sigismember(&oact.sa_mask, SIGTERM));

    act.sa_handler = SIG_IGN;
    ZASSERT(sigaction(SIGPIPE, &act, NULL) == 0);

    act.sa_handler = sighandler;
    ZASSERT(sigaction(SIGPIPE, &act, NULL) == 0);
    ZASSERT(sigaction(SIGPIPE, NULL, &oact) == 0);
    ZASSERT(oact.sa_handler == sighandler);
    ZASSERT(oact.sa_flags == (SA_NODEFER | SA_RESTORER));
    act.sa_handler = sighandler;
    act.sa_flags = SA_SIGINFO;
    ZASSERT(sigaction(SIGPIPE, &act, NULL) == 0);
    ZASSERT(sigaction(SIGPIPE, NULL, &oact) == 0);
    ZASSERT(oact.sa_handler == sighandler);
    ZASSERT(oact.sa_flags == (SA_SIGINFO | SA_RESTORER));
    act.sa_handler = SIG_IGN;
    ZASSERT(sigaction(SIGPIPE, &act, NULL) == 0);

    struct rlimit rlim;
    ZASSERT(!getrlimit(RLIMIT_NPROC, &rlim));
    ZASSERT(!getrlimit(RLIMIT_CPU, &rlim));

    ZASSERT(umask(0644));
    ZASSERT(umask(0133) == 0644);

    struct utsname utsname;
    ZASSERT(!uname(&utsname));
    ZASSERT(strlen(utsname.sysname));
    ZASSERT(strlen(utsname.nodename));
    ZASSERT(strlen(utsname.release));
    ZASSERT(strlen(utsname.version));
    ZASSERT(strlen(utsname.machine));
    opaque((void*)strlen(utsname.domainname)); // Run for effect; the string may be empty!

    struct itimerval timerval;
    memset(&timerval, 42, sizeof(struct itimerval));
    ZASSERT(!getitimer(ITIMER_REAL, &timerval));
    ZASSERT(!timerval.it_interval.tv_sec);
    ZASSERT(!timerval.it_interval.tv_usec);
    ZASSERT(!timerval.it_value.tv_sec);
    ZASSERT(!timerval.it_value.tv_usec);

    ZASSERT(getppid());
    ZASSERT(getppid() != getpid());

    struct group* group = getgrnam("tty");
    ZASSERT(group);
    ZASSERT(!strcmp(group->gr_name, "tty"));
    ZASSERT(group->gr_gid);

    char buf[256];
    ZASSERT(!getentropy(buf, 256));
    bool all_zero = true;
    size_t index;
    for (index = 256; index--;) {
        if (buf[index])
            all_zero = false;
    }
    ZASSERT(!all_zero);

    unlink("filc/test-output/miscsyscall/readonly.txt");
    unlink("filc/test-output/miscsyscall/writeonly.txt");
    unlink("filc/test-output/miscsyscall/execonly.txt");
    fd = open("filc/test-output/miscsyscall/readonly.txt", O_CREAT | O_WRONLY, 0600);
    ZASSERT(fd > 2);
    ZASSERT(!fstat(fd, &st));
    ZASSERT(st.st_mode == (0600 | S_IFREG));
    ZASSERT(!close(fd));
    ZASSERT(!chmod("filc/test-output/miscsyscall/readonly.txt", 0400));
    fd = openat(AT_FDCWD, "filc/test-output/miscsyscall/writeonly.txt", O_CREAT | O_WRONLY, 0600);
    ZASSERT(fd > 2);
    ZASSERT(!fstat(fd, &st));
    ZASSERT(st.st_mode == (0600 | S_IFREG));
    ZASSERT(!close(fd));
    ZASSERT(!chmod("filc/test-output/miscsyscall/writeonly.txt", 0200));
    fd = open("filc/test-output/miscsyscall/execonly.txt", O_CREAT | O_WRONLY, 0600);
    ZASSERT(fd > 2);
    ZASSERT(!syscall(SYS_close, fd));
    ZASSERT(!chmod("filc/test-output/miscsyscall/execonly.txt", 0100));
    ZASSERT(!access("filc/test-output/miscsyscall/readonly.txt", F_OK));
    ZASSERT(!access("filc/test-output/miscsyscall/writeonly.txt", F_OK));
    ZASSERT(!access("filc/test-output/miscsyscall/execonly.txt", F_OK));
    ZASSERT(!access("filc/test-output/miscsyscall/readonly.txt", R_OK));
    if (is_root)
        ZASSERT(!access("filc/test-output/miscsyscall/readonly.txt", W_OK));
    else {
        ZASSERT(access("filc/test-output/miscsyscall/readonly.txt", W_OK));
        ZASSERT(errno == EACCES);
    }
    ZASSERT(access("filc/test-output/miscsyscall/readonly.txt", X_OK));
    ZASSERT(errno == EACCES);
    if (is_root)
        ZASSERT(!access("filc/test-output/miscsyscall/writeonly.txt", R_OK));
    else {
        ZASSERT(access("filc/test-output/miscsyscall/writeonly.txt", R_OK));
        ZASSERT(errno == EACCES);
    }
    ZASSERT(!access("filc/test-output/miscsyscall/writeonly.txt", W_OK));
    ZASSERT(access("filc/test-output/miscsyscall/writeonly.txt", X_OK));
    ZASSERT(errno == EACCES);
    if (is_root) {
        ZASSERT(!access("filc/test-output/miscsyscall/execonly.txt", R_OK));
        ZASSERT(!access("filc/test-output/miscsyscall/execonly.txt", W_OK));
    } else {
        ZASSERT(access("filc/test-output/miscsyscall/execonly.txt", R_OK));
        ZASSERT(errno == EACCES);
        ZASSERT(access("filc/test-output/miscsyscall/execonly.txt", W_OK));
        ZASSERT(errno == EACCES);
    }
    ZASSERT(!access("filc/test-output/miscsyscall/execonly.txt", X_OK));
    ZASSERT(!faccessat(AT_FDCWD, "filc/test-output/miscsyscall/execonly.txt", X_OK, 0));

    ts.tv_sec = 0;
    ts.tv_nsec = 1;
    ZASSERT(!nanosleep(&ts, NULL));

    ZASSERT(!utimensat(AT_FDCWD, "filc/tests/miscsyscall/testfile.txt", NULL, 0));

    ZASSERT(sysconf(_SC_NPROCESSORS_ONLN) >= 1);

    ts.tv_sec = 0;
    ts.tv_nsec = 1;
    ZASSERT(!clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL));

    ZASSERT(getrandom(buf, 256, 0) == 256);
    all_zero = true;
    for (index = 256; index--;) {
        if (buf[index])
            all_zero = false;
    }
    ZASSERT(!all_zero);

    ZASSERT(syscall(SYS_getrandom, buf, 256, 0) == 256);
    all_zero = true;
    for (index = 256; index--;) {
        if (buf[index])
            all_zero = false;
    }
    ZASSERT(!all_zero);

    ZASSERT(!zis_unsafe_signal_for_kill(SIGUSR1));
    ZASSERT(!zis_unsafe_signal_for_handlers(SIGUSR1));
    ZASSERT(!zis_unsafe_signal_for_kill(SIGTRAP));
    ZASSERT(zis_unsafe_signal_for_handlers(SIGTRAP));
    ZASSERT(!zis_unsafe_signal_for_kill(SIGSYS));
    ZASSERT(!zis_unsafe_signal_for_handlers(SIGSYS));

    struct statfs sfs;
    memset(&sfs, 0, sizeof(sfs));
    ZASSERT(!statfs(".", &sfs));
    ZASSERT(sfs.f_bsize);
    ZASSERT(sfs.f_blocks);
    ZASSERT(sfs.f_namelen);
    struct statvfs svfs;
    memset(&svfs, 0, sizeof(svfs));
    ZASSERT(!statvfs(".", &svfs));
    ZASSERT(svfs.f_bsize);
    ZASSERT(svfs.f_blocks);
    ZASSERT(svfs.f_namemax);
    fd = open("filc/tests/miscsyscall/testfile.txt", O_RDONLY);
    ZASSERT(fd > 2);
    memset(&sfs, 0, sizeof(sfs));
    ZASSERT(!fstatfs(fd, &sfs));
    ZASSERT(sfs.f_bsize);
    ZASSERT(sfs.f_blocks);
    ZASSERT(sfs.f_namelen);
    memset(&svfs, 0, sizeof(svfs));
    ZASSERT(!fstatvfs(fd, &svfs));
    ZASSERT(svfs.f_bsize);
    ZASSERT(svfs.f_blocks);
    ZASSERT(svfs.f_namemax);
    close(fd);

    ZASSERT(umask(0033) == 0133);

    int miscsyscallfd = open("filc/test-output/miscsyscall", O_DIRECTORY);
    ZASSERT(miscsyscallfd > 2);
    unlinkat(miscsyscallfd, "dir1/test.txt", 0);
    unlinkat(miscsyscallfd, "dir2/test.txt", 0);
    unlinkat(miscsyscallfd, "dir2/test2.txt", 0);
    unlinkat(miscsyscallfd, "dir2/test3.txt", 0);
    unlinkat(miscsyscallfd, "dir1", AT_REMOVEDIR);
    unlinkat(miscsyscallfd, "dir2", AT_REMOVEDIR);
    ZASSERT(!mkdirat(miscsyscallfd, "dir1", 0700));
    ZASSERT(!mkdirat(miscsyscallfd, "dir2", 0700));
    fd = openat(miscsyscallfd, "dir1/test.txt", O_CREAT | O_WRONLY, 0600);
    if (fd < 0)
        zprintf("fd = %d, error = %s\n", fd, strerror(errno));
    ZASSERT(fd > 2);
    close(fd);
    ZASSERT(!renameat(miscsyscallfd, "dir1/test.txt",
                      AT_FDCWD, "filc/test-output/miscsyscall/dir2/test.txt"));
    ZASSERT(openat(miscsyscallfd, "dir1/test.txt", O_RDONLY) < 0);
    fd = openat(miscsyscallfd, "dir2/test.txt", O_RDONLY);
    ZASSERT(fd > 2);
    close(fd);
    ZASSERT(!symlinkat("../dir2/test.txt", miscsyscallfd, "dir1/test.txt"));
    fd = openat(miscsyscallfd, "dir1/test.txt", O_RDONLY);
    ZASSERT(fd > 2);
    close(fd);
    ZASSERT(!link("filc/test-output/miscsyscall/dir2/test.txt", "filc/test-output/miscsyscall/dir2/test2.txt"));
    fd = openat(miscsyscallfd, "dir2/test2.txt", O_RDONLY);
    ZASSERT(fd > 2);
    close(fd);
    ZASSERT(!linkat(miscsyscallfd, "dir2/test.txt", miscsyscallfd, "dir2/test3.txt", 0));
    fd = openat(miscsyscallfd, "dir2/test3.txt", O_RDONLY);
    ZASSERT(fd > 2);
    close(fd);

    unlink("filc/test-output/miscsyscall/dir1/test.txt");
    unlink("filc/test-output/miscsyscall/dir2/test.txt");
    unlink("filc/test-output/miscsyscall/dir2/test2.txt");
    unlink("filc/test-output/miscsyscall/dir2/test3.txt");
    rmdir("filc/test-output/miscsyscall/dir1");
    rmdir("filc/test-output/miscsyscall/dir2");
    unlink("filc/test-output/miscsyscall/dir2-link");
    ZASSERT(!mkdir("filc/test-output/miscsyscall/dir1", 0700));
    ZASSERT(!mkdir("filc/test-output/miscsyscall/dir2", 0700));
    ZASSERT(!symlink("dir2", "filc/test-output/miscsyscall/dir2-link"));
    ZASSERT(readlink("filc/test-output/miscsyscall/dir2-link", buf, sizeof(buf)) == strlen("dir2"));
    ZASSERT(!memcmp(buf, "dir2", strlen("dir2")));
    ZASSERT(!rename("filc/test-output/miscsyscall/dir2-link",
                    "filc/test-output/miscsyscall/dir2-link2"));
    bzero(buf, sizeof(buf));
    ZASSERT(readlink("filc/test-output/miscsyscall/dir2-link2", buf, sizeof(buf)) == strlen("dir2"));
    ZASSERT(!memcmp(buf, "dir2", strlen("dir2")));
    bzero(buf, sizeof(buf));
    ZASSERT(readlinkat(AT_FDCWD, "filc/test-output/miscsyscall/dir2-link2", buf, sizeof(buf)) == strlen("dir2"));
    ZASSERT(!memcmp(buf, "dir2", strlen("dir2")));

    cpu_set_t cpuset;
    ZASSERT(!sched_getaffinity(getpid(), sizeof(cpuset), &cpuset));
    ZASSERT(!sched_setaffinity(getpid(), sizeof(cpuset), &cpuset));

    // I'm going to have to revisit how to test tcget/setattr, since it behaves differently depending
    // on reasons.
    //struct termios tios;
    //ZASSERT(!tcgetattr(0, &tios));
    //ZASSERT(!tcsetattr(0, TCSANOW, &tios));
    
    ZASSERT(getauxval(AT_ENTRY));
    ZASSERT(getauxval(AT_MINSIGSTKSZ));

    int prio = getpriority(PRIO_PROCESS, 0);
    ZASSERT(prio >= 0);
    ZASSERT(!setpriority(PRIO_PROCESS, 0, prio));

    struct tms tms;
    ZASSERT(times(&tms) != (clock_t)-1);
    ZASSERT(!tms.tms_cutime);
    ZASSERT(!tms.tms_cstime);

    ZASSERT(getcwd(NULL, 0)[0] == '/');

    char buf2[10000];
    ZASSERT(getcwd(buf2, sizeof(buf2)) == buf2);
    ZASSERT(buf2[0] == '/');

    ZASSERT(dup(-1) == -1);
    ZASSERT(errno == EBADF);
    ZASSERT(dup2(-1, -1) == -1);
    ZASSERT(errno == EBADF);
    ZASSERT(fcntl(-1, F_DUPFD) == -1);
    ZASSERT(errno == EBADF);
    ZASSERT(fcntl(-1, F_DUPFD_CLOEXEC) == -1);
    ZASSERT(errno == EBADF);

    struct rusage usage;
    ZASSERT(!getrusage(RUSAGE_SELF, &usage));

    ZASSERT(select(-1, NULL, NULL, NULL, NULL) == -1);
    ZASSERT(errno == EINVAL);
    ZASSERT(pselect(-1, NULL, NULL, NULL, NULL, NULL) == -1);
    ZASSERT(errno == EINVAL);

    // Test splice and tee syscalls
    int pipe1[2], pipe2[2], pipe3[2];
    ZASSERT(!pipe(pipe1));
    ZASSERT(!pipe(pipe2));
    ZASSERT(!pipe(pipe3));

    // Test splice: write to pipe1, splice to pipe2, read from pipe2
    const char* test_data = "Hello splice!";
    size_t test_len = strlen(test_data);
    ZASSERT(write(pipe1[1], test_data, test_len) == (ssize_t)test_len);
    
    ssize_t spliced = splice(pipe1[0], NULL, pipe2[1], NULL, test_len, 0);
    ZASSERT(spliced == (ssize_t)test_len);
    
    char splice_buf[100];
    ZASSERT(read(pipe2[0], splice_buf, sizeof(splice_buf)) == (ssize_t)test_len);
    ZASSERT(!memcmp(splice_buf, test_data, test_len));

    // Test tee: write to pipe1, tee to pipe2 (data remains in pipe1), read from both
    const char* tee_data = "Hello tee!";
    size_t tee_len = strlen(tee_data);
    ZASSERT(write(pipe1[1], tee_data, tee_len) == (ssize_t)tee_len);
    
    ssize_t teed = tee(pipe1[0], pipe3[1], tee_len, 0);
    ZASSERT(teed == (ssize_t)tee_len);
    
    char tee_buf1[100], tee_buf2[100];
    ZASSERT(read(pipe3[0], tee_buf1, sizeof(tee_buf1)) == (ssize_t)tee_len);
    ZASSERT(!memcmp(tee_buf1, tee_data, tee_len));
    
    ZASSERT(read(pipe1[0], tee_buf2, sizeof(tee_buf2)) == (ssize_t)tee_len);
    ZASSERT(!memcmp(tee_buf2, tee_data, tee_len));

    // Clean up pipes
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);
    close(pipe3[0]);
    close(pipe3[1]);

    // Test splice with file descriptors and offsets
    unlink("filc/test-output/miscsyscall/splice_src.txt");
    unlink("filc/test-output/miscsyscall/splice_dst.txt");
    
    // Create source file with test data
    int src_fd = open("filc/test-output/miscsyscall/splice_src.txt", O_CREAT | O_RDWR, 0600);
    ZASSERT(src_fd > 2);
    const char* file_data = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t file_len = strlen(file_data);
    ZASSERT(write(src_fd, file_data, file_len) == (ssize_t)file_len);
    
    // Create destination file
    int dst_fd = open("filc/test-output/miscsyscall/splice_dst.txt", O_CREAT | O_RDWR | O_TRUNC, 0600);
    ZASSERT(dst_fd > 2);
    
    // Create a pipe for splicing
    int splice_pipe[2];
    ZASSERT(!pipe(splice_pipe));
    
    // Test 1: Splice from file with offset to pipe
    loff_t in_offset = 10; // Start from 'A'
    ssize_t splice_count = 10; // Splice "ABCDEFGHIJ"
    ssize_t spliced_from_file = splice(src_fd, &in_offset, splice_pipe[1], NULL, splice_count, 0);
    ZASSERT(spliced_from_file == splice_count);
    ZASSERT(in_offset == 20); // Offset should be updated
    
    // Test 2: Splice from pipe to file with offset
    loff_t out_offset = 5; // Write at position 5 in destination
    ssize_t spliced_to_file = splice(splice_pipe[0], NULL, dst_fd, &out_offset, splice_count, 0);
    ZASSERT(spliced_to_file == splice_count);
    ZASSERT(out_offset == 15); // Offset should be updated
    
    // Verify destination file content
    char verify_buf[100];
    memset(verify_buf, 0, sizeof(verify_buf));
    lseek(dst_fd, 0, SEEK_SET);
    ZASSERT(read(dst_fd, verify_buf, sizeof(verify_buf)) >= 15);
    // First 5 bytes should be zeros, then "ABCDEFGHIJ"
    ZASSERT(verify_buf[0] == 0 && verify_buf[4] == 0);
    ZASSERT(!memcmp(verify_buf + 5, "ABCDEFGHIJ", 10));
    
    // Test 3: Splice more data from different offset
    in_offset = 0; // Start from '0' at the beginning
    splice_count = 10; // Splice "0123456789"
    spliced_from_file = splice(src_fd, &in_offset, splice_pipe[1], NULL, splice_count, 0);
    ZASSERT(spliced_from_file == splice_count);
    ZASSERT(in_offset == 10); // Offset should be updated
    
    out_offset = 15; // Write right after the previous content
    spliced_to_file = splice(splice_pipe[0], NULL, dst_fd, &out_offset, splice_count, 0);
    ZASSERT(spliced_to_file == splice_count);
    ZASSERT(out_offset == 25); // Offset should be updated
    
    // Verify the new content
    memset(verify_buf, 0, sizeof(verify_buf));
    lseek(dst_fd, 0, SEEK_SET);
    ZASSERT(read(dst_fd, verify_buf, sizeof(verify_buf)) >= 25);
    // Should have zeros, then "ABCDEFGHIJ", then "0123456789"
    ZASSERT(!memcmp(verify_buf + 5, "ABCDEFGHIJ", 10));
    ZASSERT(!memcmp(verify_buf + 15, "0123456789", 10));
    
    // Test 4: Splice without offset pointer (should use current file position)
    lseek(src_fd, 0, SEEK_SET); // Reset to beginning
    splice_count = 5;
    spliced_from_file = splice(src_fd, NULL, splice_pipe[1], NULL, splice_count, 0);
    ZASSERT(spliced_from_file == splice_count);
    
    // Read and verify it's from the beginning
    char no_offset_buf[10];
    ZASSERT(read(splice_pipe[0], no_offset_buf, splice_count) == splice_count);
    ZASSERT(!memcmp(no_offset_buf, "01234", 5));
    
    // Clean up
    close(src_fd);
    close(dst_fd);
    close(splice_pipe[0]);
    close(splice_pipe[1]);

    unlink("filc/test-output/miscsyscall/testnode");
    ZASSERT(!mknod("filc/test-output/miscsyscall/testnode", 0600, S_IFREG));
    fd = open("filc/test-output/miscsyscall/testnode", O_RDONLY);
    ZASSERT(fd > 2);
    ZASSERT(!read(fd, buf, 1));
    ZASSERT(!close(fd));

    ZASSERT(sched_getcpu() >= 0);

    sched_yield();
    thrd_yield();

    ZASSERT(secure_getenv("PATH"));

    int result;
#ifndef __USE_GNU
    struct winsize ws;
    result = tcgetwinsize(0, &ws);
    ZASSERT(!result || (result == -1 && errno == ENOTTY));
#endif

    struct timex timex;
    adjtimex(&timex);

    struct sched_param param;
    pthread_setschedparam(pthread_self(), 0, &param);
    int policy;
    pthread_getschedparam(pthread_self(), &policy, &param);

    ZASSERT(!posix_madvise(zgc_aligned_alloc(4096, 4096), 4096, POSIX_MADV_NORMAL));
    ZASSERT(!posix_madvise(zgc_aligned_alloc(4096, 4096), 4096, POSIX_MADV_DONTNEED));

    ZASSERT(prctl(PR_GET_DUMPABLE) != -1);
    ZASSERT(prctl(PR_GET_NO_NEW_PRIVS) != -1);
    ZASSERT(prctl(PR_GET_SECCOMP) != -1);
    ZASSERT(prctl(PR_GET_TIMERSLACK) != -1);
    ZASSERT(prctl(PR_GET_FP_MODE) == -1);
    ZASSERT(errno == EINVAL);
    result = prctl(PR_GET_IO_FLUSHER);
    if (result == -1)
        ZASSERT(errno == EPERM);
    else
        ZASSERT(!result || result == 1);
    ZASSERT(prctl(PR_GET_KEEPCAPS) != -1);
    ZASSERT(prctl(PR_GET_SECUREBITS) != -1);
    result = prctl(PR_SVE_GET_VL);
    if (result == -1)
        ZASSERT(errno == EINVAL);
    ZASSERT(prctl(PR_GET_TAGGED_ADDR_CTRL) == -1);
    ZASSERT(errno == EINVAL);
    ZASSERT(prctl(PR_GET_THP_DISABLE) != -1);
    ZASSERT(prctl(PR_GET_TIMING) != -1);

    fd = memfd_create(zasprintf("miscsyscall-%d", getpid()), 0);
    ZASSERT(fd >= 2);
    ZASSERT(!close(fd));
    fd = syscall(SYS_memfd_create, zasprintf("miscsyscall-%d", getpid()), 0);
    ZASSERT(fd >= 2);
    ZASSERT(!close(fd));

    ZASSERT(signal(SIGILL, SIG_IGN) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGTRAP, SIG_IGN) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGBUS, SIG_IGN) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGSEGV, SIG_IGN) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGFPE, SIG_IGN) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGSYS, SIG_IGN) == SIG_DFL);
    ZASSERT(signal(SIGILL, sighandler) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGTRAP, sighandler) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGBUS, sighandler) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGSEGV, sighandler) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGFPE, sighandler) == SIG_ERR);
    ZASSERT(errno == ENOSYS);
    ZASSERT(signal(SIGSYS, sighandler) == SIG_IGN);

    zprintf("No worries.\n");
    return 0;
}

