// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test for --history trail ORDER: poison a value (cannot-index), load it, fork
// its provenance into two registers, and merge the forks with an OR as a
// memory index -> fault; its trail is the set-union of the forks plus the OR
// PC. The trail VECTOR is stored in MERGE order -- measured stored sequence
// [poison-call, A, F, B, C, G, D, E, mask] (see the run-tests history-order
// case) -- which interleaves the chronological op-chains. Each op line below
// ends in a bracket listing the CHRONOLOGICAL op-chain of the guest ops so far
// (poison-call excluded). The ops are single-instruction inline asm on
// explicit local register variables (gcc pins values across asm statements),
// one per line, so the dump must print the steps in strictly ascending source
// line order: every step consumes values produced at earlier steps;
// consecutive steps may assign the same register when the program chains
// back-to-back operations on it (an rmw chain -- this guest trail contains
// four such pairs). Strict per-step chaining is NOT guaranteed when provenance
// forks (each merged trail is the union of its lineages): step E (andl $0x2a
// on b) is followed by step F (andl %ecx,%ebx), which consumes a/c, not that b.
// The recorded trail PCs symbolicate to distinct ascending lines.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(

        op, ptr, size, flags);
}

static volatile char src[64];
static volatile char sinkbuf[64];
static volatile char sink;

int main(void) {
    magic(1, (void*)src, sizeof(src), 1u << 3); // cannot-index poison
    volatile char* sp = (volatile char*)src;
    register uint64_t a asm("rbx") = 0;
    register uint64_t b asm("rsi") = 0;
    register uint64_t c asm("rcx") = 0;
    __asm__ __volatile__("xorl %k0, %k0" : "+r"(c) : : ); // start clean
    __asm__ __volatile__("movzbl (%1), %k0" : "=r"(a) : "r"(sp) : ); // A: tainted load -> chronological op-chain [A]
    __asm__ __volatile__("xorl %k1, %k0" : "+r"(c) : "r"(a) : ); // B: c = a ^ c -> [A,B]
    __asm__ __volatile__("notl %k0" : "+r"(c) : : ); // C: -> [A,B,C]
    __asm__ __volatile__("movl %k1, %k0" : "=r"(b) : "r"(c) : ); // D: b = c -> [A,B,C,D]
    __asm__ __volatile__("andl $0x2a, %k0" : "+r"(b) : : ); // E: -> [A,B,C,D,E]
    __asm__ __volatile__("andl %k1, %k0" : "+r"(a) : "r"(c) : ); // F: a = a & c -> [A,B,C,D,E,F]
    __asm__ __volatile__("orl %k1, %k0" : "+r"(a) : "r"(b) : ); // G: merge! -> [A,B,C,D,E,F,G]
    sink = sinkbuf[a & 7]; // H: index use -> FAULT
    write(1, "NOTREACHED\n", 11);
    return 0;
}
