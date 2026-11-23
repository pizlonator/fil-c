// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: the soundness half of kernel-write extent modeling -- kernel-
// UNTOUCHED input buffers must STAY poisoned. Poisons a buffer holding an
// itimerspec that is passed as the new_value INPUT of timerfd_settime
// (with old_value NULL, so the syscall writes nothing to guest memory),
// then branches on the input bytes: the kernel only READS new_value, so
// the bytes must remain cannot-branch tainted and the branch must fault
// (exit 139, core). Clearing the input would launder kernel-untouched
// poison -- the old timerfd_settime extent bug modeled exactly that by
// clearing the caller-provided new_value instead of the old_value
// writeback. Mirrors syscall_unpoison.c: a magic-call guest that only
// runs under the emulator (natively the magic address is not mapped).
#define _GNU_SOURCE
#include <stdint.h>

#include <stdio.h>

#include <sys/timerfd.h>

#include <time.h>

#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

// The poisoned arena; the itimerspec lives at buf+64.
static volatile char buf[256];

int main(void) {
    magic(1, (void*)buf, sizeof(buf), 1u << 2); // POISON_CANNOT_BRANCH

    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd < 0)
        return 1;

    // Fill the new_value INPUT (stores do not sanitize CANNOT_BRANCH
    // poison: the bytes stay tainted through the syscall).
    struct itimerspec* newv = (struct itimerspec*)(buf + 64);
    newv->it_interval.tv_sec = 0;
    newv->it_interval.tv_nsec = 0;
    newv->it_value.tv_sec = 0;
    newv->it_value.tv_nsec = 100000000;

    // old_value is NULL: the kernel writes nothing into guest memory, so
    // no extent may be pushed anywhere -- least of all over the input.
    if (timerfd_settime(tfd, 0, newv, NULL) != 0)
        return 2;

    // Branch on the INPUT bytes the kernel never wrote: must fault with
    // the cannot-branch reason.
    if (*(volatile long*)&newv->it_value.tv_nsec != 100000000 ||
        *(volatile long*)&newv->it_value.tv_sec != 0)
        return 3;

    write(1, "ctl-ok\n", 7); // never reached: the branch above faults
    return 0;
}
