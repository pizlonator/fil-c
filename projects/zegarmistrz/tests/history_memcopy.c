// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test for --history memory-side provenance: poison a buffer, copy it to a
// second buffer byte by byte (load+store round trips), reload from the
// copy, then use the reloaded value as a memory index -> fault. The dumped
// trail must span the whole journey: the poisoning call site, the copy
// loop's load and store PCs, the reload PC, and the faulting use.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile char src[64];
static volatile char dst[64];
static volatile char sinkbuf[64];
static volatile char sink;

int main(void) {
    magic(1, (void*)src, sizeof(src), 1u << 3);      // cannot-index poison
    for (int i = 0; i < (int)sizeof(src); i++)       // copy loop: load+store round trips
        dst[i] = src[i];
    volatile uint64_t x = *(volatile uint64_t*)(dst + 8); // reload from the copy
    sink = *(volatile char*)((volatile char*)sinkbuf + (x & 7)); // index use: FAULT
    write(1, "NOTREACHED\n", 11);
    return 0;
}
