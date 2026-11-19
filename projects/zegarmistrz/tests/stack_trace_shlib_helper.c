// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Helper for the stack-trace-shlib case: built as a shared library that the
// guest ld.so maps at runtime; shlib_boom poisons buf (client request) and
// then faults reading it, so the crash-time guest stack trace must
// symbolicate the faulting frame inside this shared object.
#include <stddef.h>
#include <stdint.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

__attribute__((noinline)) void shlib_boom(volatile char* buf, size_t n) {
    magic(1, (void*)buf, n, 1u); // poison cannot-load
    char c = buf[0];             // must fault inside this shared object
    (void)c;
}
