#include <stdio.h>

long fnG(unsigned long which, unsigned long idx);

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d\n", __FILE__, __LINE__); fails++; } } while (0)

int main() {
    // which == 0 skips the `and` (an and-free path); which != 0 executes the
    // dynamic alignment. Both paths reach the same buffer accesses with the
    // same modeled offsets, so both must read the same bytes.
    CHECK(fnG(0, 0) == (long)(0x11111111u + 0x22222222u));
    CHECK(fnG(0, 8) == (long)(0x33333333u + 0x22222222u));
    CHECK(fnG(1, 0) == (long)(0x11111111u + 0x22222222u));
    CHECK(fnG(1, 8) == (long)(0x33333333u + 0x22222222u));
    CHECK(fnG(0, 12) == fnG(1, 12));
    if (fails == 0) printf("stack buffer andframe ok\n");
    return fails ? 1 : 0;
}
