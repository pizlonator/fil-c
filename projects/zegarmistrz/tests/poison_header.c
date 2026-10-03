// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: include/zegarmistrz.h poison via the header-only library. Natively
// every service call is a no-op (exit 0). Under the emulator, poisoning
// cannot-store through the header must really reach the emulator, so the
// store below faults (guest SIGSEGV -> exit 139 + core dump).
#include <unistd.h>

#include "zegarmistrz.h"

static volatile char buf[64];

int main(void) {
    if (!is_in_zegarmistrz())
        return 0;
    if (zegarmistrz_poison_range((void*)buf, sizeof(buf),
                                 ZEGARMISTRZ_POISON_CANNOT_STORE) != 0)
        return 1;
    buf[0] = 'x'; // must fault under the emulator (cannot-store)
    return 0;
}
