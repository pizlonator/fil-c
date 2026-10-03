#include <stdio.h>

long sbdepth0_test(unsigned long idx, unsigned long word);

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d\n", __FILE__, __LINE__); fails++; } } while (0)

// Canary wall against buffer-region escape, swept through a full alignment
// period by C recursion (64 levels): each level owns a 256-byte pattern-filled
// wall segment in its own frame, directly above the test call's entry rsp,
// calls the test, and verifies the segment. The segment is `volatile` with its
// address escaped through an asm barrier, so it stays a real live
// across-the-call stack object; bytes [0..16) are never verified (frame-bottom
// call scratch may legally live there). The data checks run on every level.
static void run(int k) {
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 64) run(k + 1);
    CHECK(sbdepth0_test(0, 0) == (long)(0x11111111u + 0x11111111u));
    CHECK(sbdepth0_test(4, 0) == (long)(0x22222222u + 0x11111111u));
    CHECK(sbdepth0_test(0, 1) == (long)(0x11111111u + 0x22222222u));
    CHECK(sbdepth0_test(8, 2) == (long)(0x33333333u + 0x33333333u));
    CHECK(sbdepth0_test(12, 3) == (long)(0x44444444u + 0x44444444u));
    // Exact upper boundaries of the two bounds checks: idx = 60 = 64 - 4 for
    // the byte-index form, word = 15 = 64/4 - 1 for the scale-4 form.
    CHECK(sbdepth0_test(60, 15) == (long)(0x55555555u + 0x55555555u));
    CHECK(sbdepth0_test(60, 0) == (long)(0x55555555u + 0x11111111u));
    CHECK(sbdepth0_test(0, 15) == (long)(0x11111111u + 0x55555555u));
    for (int i = 16; i < 256; ++i) {
        if (wallseg[i] != (char)(0xA5 + i)) {
            printf("WALL corrupt at depth %d byte %d\n", k, i);
            fails++;
            return;
        }
    }
}

int main() {
    run(0);
    if (fails == 0) printf("stack buffer anddepth0 ok\n");
    return fails ? 1 : 0;
}
