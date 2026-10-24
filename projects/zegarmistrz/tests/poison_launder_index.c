// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: laundering acquires the taint -- poison cannot-index WITHOUT
// clear-index-on-store, store a clean pointer through it, load it back,
// then dereference it -> guest error + core dump. The store must NOT
// sanitize the poison; only clear-index-on-store does that.

#include <stdint.h>

#include <stdio.h>

#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static char target = 'T';

int main(void) {
    static char* volatile slot = 0;
    // (1) Poison CANNOT_INDEX only: no CLEAR_INDEX_ON_STORE.
    magic(1, (void*)&slot, sizeof(slot), 8u);
    // (2) Store a clean pointer into the poisoned slot: must not fault
    // and must not clear the poison.
    slot = &target;
    // (3) Load it back: the pointer acquires the cannot-index taint.
    char* q = slot; // volatile load through poisoned memory
    // (4) Dereference the loaded pointer -> guest error.
    volatile char c = *q;
    (void)c;
    write(1, "NOTREACHED\n", 11);
    return 0;
}
