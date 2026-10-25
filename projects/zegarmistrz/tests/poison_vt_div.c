// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: cannot-var-time-op laundering -- poison a slot with
// CANNOT_VAR_TIME_OP only, store clean data through it (the store must NOT
// sanitize without clear-var-time-op-on-store), load it back (the loaded
// value acquires the taint), then divide by it -> the DIV's timing would
// depend on the tainted operand (doc 4.1), so the guest must fault with the
// var-time reason + core dump.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t clean_dividend = 0x1122334455667788ULL;

int main(void) {
    static uint64_t volatile slot = 0;
    // (1) Poison CANNOT_VAR_TIME_OP only: no CLEAR_VAR_TIME_OP_ON_STORE.
    magic(1, (void*)&slot, sizeof(slot), 64u);
    // (2) Store clean data into the poisoned slot: must not fault and must
    // not clear the poison.
    slot = clean_dividend;
    // (3) Load it back: the bytes acquire the cannot-var-time-op taint.
    uint64_t d = slot; // volatile load through poisoned memory
    // (4) Divide by the tainted value: the DIV's execution time depends on
    // the operand values (doc 4.1) -> guest error. clean_dividend is a
    // runtime-unknown volatile so gcc -O2 emits a real DIV.
    volatile uint64_t q = clean_dividend / d;
    (void)q;
    write(1, "NOTREACHED\n", 11);
    return 0;
}