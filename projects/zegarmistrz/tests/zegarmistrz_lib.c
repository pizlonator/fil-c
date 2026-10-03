// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: the include/zegarmistrz.h header-only client library. Under the
// emulator: detection is true and the service calls work (query, poison,
// unpoison). Natively: detection is false and every service call is a
// no-op; the program must still exit 0. Prints "zeglib-zegarmistrz" under
// the emulator, "zeglib-native" otherwise; exit 0 in both cases.
#include <stdint.h>
#include <unistd.h>

#include "zegarmistrz.h"

static volatile char buf[64];

int main(void) {
    if (is_in_zegarmistrz()) {
        if (zegarmistrz_query() != 0)
            return 1;
        // Poison cannot-index + clear-index-on-store on a pointer slot,
        // then sanitize it by storing a clean pointer, then deref.
        static char target = 'T';
        static char* volatile ptr;
        if (zegarmistrz_poison_range((void*)&ptr, sizeof(ptr),
                                     ZEGARMISTRZ_POISON_CANNOT_INDEX |
                                         ZEGARMISTRZ_POISON_CLEAR_INDEX_ON_STORE) != 0)
            return 2;
        ptr = &target; // clean store: succeeds and sanitizes the slot
        const char* q = ptr; // load must be clean now
        if (*q != 'T')
            return 3;
        // Poison + unpoison around a plain write.
        if (zegarmistrz_poison_range((void*)buf, sizeof(buf),
                                     ZEGARMISTRZ_POISON_CANNOT_LOAD |
                                         ZEGARMISTRZ_POISON_CANNOT_STORE) != 0)
            return 4;
        if (zegarmistrz_unpoison_range((void*)buf, sizeof(buf)) != 0)
            return 5;
        buf[0] = 'k';
        if (buf[0] != 'k')
            return 6;
        write(1, "zeglib-zegarmistrz\n", 19);
        return 0;
    }
    // Not in zegarmistrz: all service calls must be safe no-ops.
    if (zegarmistrz_query() != 0)
        return 7;
    (void)zegarmistrz_poison_range((void*)buf, sizeof(buf),
                                   ZEGARMISTRZ_POISON_CANNOT_LOAD);
    (void)zegarmistrz_unpoison_range((void*)buf, sizeof(buf));
    buf[0] = 'n';
    if (buf[0] != 'n')
        return 8;
    write(1, "zeglib-native\n", 14);
    return 0;
}
