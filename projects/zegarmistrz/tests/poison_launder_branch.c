// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: laundering acquires the taint -- poison cannot-branch WITHOUT
// clear-branch-on-store, store clean data through it, load it back, then
// branch on what was loaded -> guest error + core dump. The store must
// NOT sanitize the poison; only clear-branch-on-store does that.

#include <stdint.h>

#include <stdio.h>

#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static uint64_t clean_data = 0x1122334455667788ULL;

int main(void) {
    static uint64_t volatile slot = 0;
    // (1) Poison CANNOT_BRANCH only: no CLEAR_BRANCH_ON_STORE.
    magic(1, (void*)&slot, sizeof(slot), 4u);
    // (2) Store clean data into the poisoned slot: must not fault and
    // must not clear the poison.
    slot = clean_data;
    // (3) Load it back: the bytes acquire the cannot-branch taint.
    uint64_t v = slot; // volatile load through poisoned memory
    // (4) Branch on what we loaded -> guest error.
    if (v == clean_data)
        write(1, "taken\n", 6);
    else
        write(1, "not taken\n", 10);
    write(1, "NOTREACHED\n", 11);
    return 0;
}
