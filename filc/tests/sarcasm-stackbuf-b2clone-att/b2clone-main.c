#include <stdio.h>

long fnA(unsigned long idx);
long fnA2(unsigned long idx);
long fnB(unsigned long idx);

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d\n", __FILE__, __LINE__); fails++; } } while (0)

int main() {
    // The tail's owner, called directly: the buffer lives in fnB's own frame.
    CHECK(fnB(0) == (long)(0x11111111u + 0x22222222u));
    CHECK(fnB(8) == (long)(0x33333333u + 0x22222222u));
    // idx = 60 is the exact upper boundary for a 4-byte access into 64 bytes.
    CHECK(fnB(60) == (long)(0x55555555u + 0x22222222u));

    // fnA's B2 clone of fnB's body+tail: the same declaration resolved in the
    // clone's own context, lowered into fnA's own output frame. Every byte the
    // clone wrote (0, 4, 8, 60) reads back identically.
    CHECK(fnA(0) == (long)(0x11111111u + 0x22222222u));
    CHECK(fnA(8) == (long)(0x33333333u + 0x22222222u));
    CHECK(fnA(60) == (long)(0x55555555u + 0x22222222u));

    // fnA2's clone is independent: same region semantics, its own frame, its
    // own bounds check. The zero-length short-circuit path never enters the
    // clone (fnA2 returns 0 there by its own construction).
    CHECK(fnA2(0) == 0);
    CHECK(fnA2(4) == (long)(0x22222222u + 0x22222222u));
    CHECK(fnA2(60) == (long)(0x55555555u + 0x22222222u));
    CHECK(fnA2(4) == fnA(4));
    CHECK(fnA2(60) == fnA(60));

    // Interleaved calls: each clone's buffer is per-call frame memory, so
    // interleaving cannot cross-contaminate.
    CHECK(fnA(0) == fnB(0));
    CHECK(fnA2(8) == fnB(8));
    CHECK(fnB(0) == fnA(0));

    if (fails == 0) printf("stack buffer b2clone ok\n");
    return fails ? 1 : 0;
}
