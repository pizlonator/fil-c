// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: storing a tainted value more than once to the same memory slot (e.g.
// spilling a secret variable to the same stack slot across loop iterations)
// must keep the slot tainted rather than toggling taint off via clear-on-store.
// Expect exit 139 with cannot-branch.
#include <stdint.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

int main(void) {
    static volatile uint64_t secret = 0x123456789abcdef0ULL;
    static volatile uint64_t slot = 0;

    // Poison secret with CANNOT_BRANCH (bit 2).
    magic(1, (void*)&secret, sizeof(secret), 4u);

    uint64_t v = secret;
    // First spill sets CANNOT_BRANCH | CLEAR_BRANCH_ON_STORE on slot.
    slot = v;
    // Second tainted spill to the same slot must keep CANNOT_BRANCH set.
    slot = v + 1;

    uint64_t reloaded = slot;
    if (reloaded == 0)
        return 1;
    if (write(1, "missed-respill\n", 15) < 0)
        return 2;
    return 0;
}
