// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: multi-byte NOPs (the 0f 1f /0 family and friends) encode an address
// purely as padding -- the operand is decorative and the instruction never
// touches memory -- so a cannot-index-tainted base or index register must
// NOT fault, in every addressing form (base, disp8, disp32, base+index,
// index*scale, index-only, 66/REX.W prefixed). Runs only under the emulator:
// must exit 0 and print "nop-ok".

#include <stdint.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t base_slot = 24;
static volatile uint64_t idx_slot = 3;

int main(void) {
    magic(1, (void*)&base_slot, sizeof(base_slot), 8u); // op=poison, cannot-index
    magic(1, (void*)&idx_slot, sizeof(idx_slot), 8u);
    uint64_t t = base_slot; // tainted 24
    uint64_t i = idx_slot;  // tainted 3
    asm volatile("nopw 0x0(%0,%0,1)" :: "r"(t) : "memory");              // 66 0f 1f 44 00 00
    asm volatile("nopl 0x0(%0)" :: "r"(t) : "memory");                   // 0f 1f 40 00
    asm volatile("nopl 0x40(%0)" :: "r"(t) : "memory");                  // 0f 1f 40 40
    asm volatile("nopl 0x11223344(%0)" :: "r"(t) : "memory");            // 0f 1f 80 <disp32>
    asm volatile("nopw 0x40(%0)" :: "r"(t) : "memory");
    asm volatile("nopl (%0,%1)" :: "r"(t), "r"(i) : "memory");           // base+index
    asm volatile("nopl 0x40(%0,%1,8)" :: "r"(t), "r"(i) : "memory");     // disp8 + scale8
    asm volatile("nopl 0x11223344(,%1,4)" :: "r"(t), "r"(i) : "memory"); // index-only
    asm volatile("nopq 0x0(%0,%0,1)" :: "r"(t) : "memory");              // 48 0f 1f 44 00 00
    write(1, "nop-ok\n", 7);
    return 0;
}
