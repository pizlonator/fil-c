#include <stdio.h>
#include <string.h>

long ba_basic(unsigned long idx);
long ba_alu(unsigned long idx);
long ba_store_reload(unsigned long idx);
long ba_heap_roundtrip(void* heap, unsigned long idx);
long ba_cmov(void* heap, unsigned long which);
long ba_walk(unsigned long idx);
long ba_masked(unsigned long idx);
long ba_masked_walk(unsigned long idx);

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d\n", __FILE__, __LINE__); fails++; } } while (0)

int main() {
    // Value lea into the buffer + a computed address: the two annotated dwords
    // at buffer bytes 8 and 12 reassemble the seeded qword.
    CHECK(ba_basic(0) == 0x1122334455667788LL);
    // ALU on the address (walking down); only written bytes are read.
    CHECK(ba_alu(0) == 0xfeedface05060708LL);
    // The buffer address stored to a frame slot and reloaded.
    CHECK(ba_store_reload(0) == 0x5a5a5a5a);
    // The buffer address stored through a real heap pointer and reloaded.
    {
        static long slot[4];
        memset(slot, 0, sizeof(slot));
        CHECK(ba_heap_roundtrip(slot, 0) == 0x11223344);
    }
    // The cmov selects the buffer path (the heap value would fail the check).
    CHECK(ba_cmov(0, 0) == 0x6a6a6a6a);
    // A walking pointer with per-store runtime checks; every byte read was
    // written first.
    CHECK(ba_walk(0) == 0x4030001);
    // A masked ({k1}) by-address store: lanes 0-3 write 34s, the rest keep 17s.
    CHECK(ba_masked(0) == 17);
    CHECK(ba_masked_walk(0) == 34);
    if (!fails) printf("stackbuf byaddr att ok\n");
    return fails ? 1 : 0;
}
