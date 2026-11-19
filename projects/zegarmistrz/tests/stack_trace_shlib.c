// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: a fatal guest error inside a shared library the guest ld.so mapped
// must symbolicate in the crash-time guest stack trace (see
// stack_trace_shlib_helper.c for the faulting function).
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

extern void shlib_boom(volatile char* buf, size_t n);

static volatile char buf[64];

int main(void) {
    shlib_boom((volatile char*)buf, sizeof(buf));
    write(1, "NOTREACHED\n", 11);
    return 0;
}
