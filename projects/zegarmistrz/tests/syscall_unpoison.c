// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: kernel-write extent modeling. A static buffer is poisoned entirely
// with cannot-branch poison; every kernel call below writes INTO that
// poisoned range (fstat's struct stat, ioctl FIONREAD's int, read's bytes,
// getrandom's bytes, recvfrom's data bytes + sockaddr + socklen word, and
// timerfd_settime's old-value itimerspec), so each kernel-written byte
// must come back fresh and non-poisoned -- branching on the kernel-written
// bytes must run clean (no "tainted (cannot-branch)" fault). A fault means
// the emulator missed the write extent of the syscall that touched those
// bytes. Prints "unpoison-ok" and exits 0 on success, mirroring the other
// clean poison-* guests. Like every magic-call guest this only runs under
// the emulator (natively the magic address is not mapped).
#define _GNU_SOURCE
#include <fcntl.h>

#include <stdint.h>

#include <stdio.h>

#include <string.h>

#include <sys/ioctl.h>

#include <sys/random.h>

#include <sys/socket.h>

#include <sys/stat.h>

#include <sys/timerfd.h>

#include <sys/un.h>

#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

// One big poisoned arena; each syscall writes at its own offset. Poisoning
// the WHOLE arena at once (rather than re-poisoning around each call) is
// what catches a missed extent: any byte the kernel fails to overwrite
// stays cannot-branch tainted and the next branch faults.
static volatile char buf[2048];

int main(void) {
    magic(1, (void*)buf, sizeof(buf), 1u << 2); // POISON_CANNOT_BRANCH

    // 1. fstat's 144-byte struct stat lands inside the poisoned arena.
    //    Branching on st_mode (the S_ISCHR check __GI__IO_file_doallocate
    //    does) must not fault: the kernel overwrote all 144 bytes.
    struct stat* st = (struct stat*)(buf + 128);
    if (fstat(1, st) != 0)
        return 1;
    if (((st->st_mode & S_IFCHR) == S_IFCHR) && st->st_rdev == 0)
        return 2; // unreachable for a pipe/tty stdout, but the branch runs

    // 2. ioctl FIONREAD on a pipe: the fixed-table ioctl path writes the
    //    queued byte count through the a3 pointer.
    int fds[2];
    if (pipe2(fds, 0) != 0)
        return 3;
    if (write(fds[1], "x", 1) != 1)
        return 4;
    if (ioctl(fds[0], FIONREAD, buf + 256) != 0)
        return 5;
    if (*(volatile int*)(buf + 256) < 1)
        return 6;

    // 3. read() fills buf[512..]: branch on the kernel-written byte.
    int fd = open("/etc/hostname", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, (void*)(buf + 512), 256);
        if (n > 0) {
            if (buf[512] == (char)0xff)
                return 7; // branch on kernel-written data
        }
        close(fd);
    }

    // 4. getrandom() fills buf[896..]: branch on the kernel-written byte.
    ssize_t gn = getrandom((void*)(buf + 896), 64, 0);
    if (gn > 0) {
        if (*(volatile char*)(buf + 896) == (char)0x42 && gn > 1000000)
            return 8; // branch on kernel-written data
    }

    // 5. recvfrom() on an AF_UNIX socketpair: the kernel writes the data
    //    byte into buf+1024, the peer address (sv[0] is bound to an
    //    ABSTRACT AF_UNIX name -- sun_path[0] == '\0', no filesystem path)
    //    into the sockaddr_storage at buf+1280, and the actual address
    //    length into the socklen_t word at buf+1504. The flags value is
    //    NONZERO (MSG_DONTWAIT): the flags VALUE must never become a write
    //    extent (the old arg-mapping bug pushed one at a4 and left the
    //    real sockaddr/addrlen writebacks poisoned), so all three branches
    //    on kernel-written bytes must run clean.
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
        return 9;
    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    sun.sun_path[0] = '\0'; // abstract: no filesystem path needed
    memcpy(sun.sun_path + 1, "zeg-unpoison", 12);
    if (bind(sv[0], (struct sockaddr*)&sun, sizeof(sun)) != 0)
        return 10;
    if (sendto(sv[0], "x", 1, 0, NULL, 0) != 1)
        return 11;
    socklen_t* lenword = (socklen_t*)(buf + 1504);
    *lenword = sizeof(struct sockaddr_storage); // input: buffer capacity
    ssize_t rn = recvfrom(sv[1], (void*)(buf + 1024), 256, MSG_DONTWAIT,
                          (struct sockaddr*)(buf + 1280), lenword);
    if (rn != 1)
        return 12;
    if (*(volatile socklen_t*)lenword < 2)
        return 13; // the kernel always writes the length (>= 2 abstract)
    if (*(volatile sa_family_t*)(buf + 1280) != AF_UNIX)
        return 14; // the kernel wrote sun_family
    if (*(volatile char*)(buf + 1024) == 'y')
        return 15; // branch on the kernel-written data byte
    close(sv[0]);
    close(sv[1]);

    // 6. timerfd_settime(): the kernel writes the PREVIOUS setting to the
    //    old_value buffer at buf+1664; new_value at buf+1600 is the
    //    caller's INPUT and the kernel never touches it. Branching on the
    //    old value must run clean (the kernel-untouched new_value staying
    //    poisoned is pinned separately by the syscall-unpoison-ctl fault
    //    case).
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd < 0)
        return 16;
    struct itimerspec* newv = (struct itimerspec*)(buf + 1600);
    struct itimerspec* oldv = (struct itimerspec*)(buf + 1664);
    newv->it_interval.tv_sec = 0;
    newv->it_interval.tv_nsec = 0;
    newv->it_value.tv_sec = 0;
    newv->it_value.tv_nsec = 100000000; // arm for 100 ms
    if (timerfd_settime(tfd, 0, newv, oldv) != 0)
        return 17;
    if (*(volatile long*)&oldv->it_value.tv_sec != 0 ||
        *(volatile long*)&oldv->it_value.tv_nsec != 0)
        return 18; // never armed before: the old value reads back zero
    close(tfd);

    write(1, "unpoison-ok\n", 12);
    return 0;
}