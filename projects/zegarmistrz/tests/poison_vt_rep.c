// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.

// Test: REP MOVS/STOS/LODS count laundering -- poison a slot with

// CANNOT_VAR_TIME_OP only, store a clean count through it (the store must

// NOT sanitize without clear-var-time-op-on-store), load it back (the

// loaded count acquires the taint), then `rep movsb` with it -> the

// iteration count makes the instruction's execution time data-dependent

// (doc 4.4 lists RCX as VAL for REP MOVS/STOS/LODS), so the guest must

// fault with the var-time reason + core dump, in the DEFAULT mode (doc 4.4

// is KNOWN).

#include <stdint.h>

#include <stdio.h>

#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {

    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(

        op, ptr, size, flags);

}

static volatile uint64_t clean_count = 8;

static char src_buf[16];

static char dst_buf[16];

int main(void) {

    static uint64_t volatile slot = 0;

    // (1) Poison CANNOT_VAR_TIME_OP only: no CLEAR_VAR_TIME_OP_ON_STORE.

    magic(1, (void*)&slot, sizeof(slot), 64u);

    // (2) Store a clean count into the poisoned slot: must not fault and

    // must not clear the poison.

    slot = clean_count;

    // (3) Load it back: the count acquires the cannot-var-time-op taint.

    size_t n = (size_t)slot; // volatile load through poisoned memory

    // (4) rep movsb with the tainted count: RCX is a VAL timing operand per

    // doc 4.4 -> guest error. gcc -O2 will not always emit rep from memcpy,

    // so the rep form is written in inline asm with valid buffers; RSI/RDI/

    // RCX are updated by the instruction, so they are in+out operands (not

    // clobbers).

    const char* s = src_buf;

    char* d = dst_buf;

    __asm__ volatile("rep movsb" : "+S"(s), "+D"(d), "+c"(n) : : "memory");

    write(1, "NOTREACHED\n", 11);

    return 0;

}
