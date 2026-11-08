// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: LEA must NOT trip the cannot-index check. LEA computes an address
// without ever touching memory -- it is pure address arithmetic (compilers
// emit it for ordinary adds like `x = y + 42`) -- so a cannot-index-tainted
// base or index register must be accepted in every addressing form, and the
// computed address must stay exact (this pins compute_ea's arithmetic).
// A %fs:/%gs: segment override must be IGNORED like on real hardware (LEA
// computes only the offset, never base+index+disp plus a segment base).
// The taint still propagates into the destination; poison_index_lea_prop.c
// covers that laundering side.
//
// Taint wrinkle: the tainted LEA results cannot be compared directly
// (cmp taints the flags and the following jcc would fault with
// "cannot-index ... in branch condition" -- a true property, not what this
// guest is about). So each result is laundered through a sink: volatile
// store, unpoison, volatile reload, then the exactness checks branch on the
// now-clean values. A regressed LEA check shows up as exit 139 here.

#include <stdint.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static char buf[256];
static volatile uint64_t base_slot = 24;
static volatile uint64_t idx_slot = 3;
static volatile uint64_t results[6];

int main(void) {
    magic(1, (void*)&base_slot, sizeof(base_slot), 8u); // op=poison, cannot-index
    magic(1, (void*)&idx_slot, sizeof(idx_slot), 8u);
    uint64_t t = base_slot; // tainted 24
    uint64_t i = idx_slot;  // tainted 3
    uint64_t out;
    int k = 0;
    // Untainted sanity: base-only. (Clean value, compare directly.)
    asm volatile("lea (%1), %0" : "=r"(out) : "r"((uint64_t)buf));
    if (out != (uint64_t)buf) return 1;
    // Tainted base-only.
    asm volatile("lea (%1), %0" : "=r"(out) : "r"(t));
    results[k++] = out;
    // Tainted base + disp8.
    asm volatile("lea 0x40(%1), %0" : "=r"(out) : "r"(t));
    results[k++] = out;
    // Tainted base + disp32 (negative).
    asm volatile("lea -0x1000(%1), %0" : "=r"(out) : "r"(t));
    results[k++] = out;
    // Tainted base + tainted index.
    asm volatile("lea (%1,%2), %0" : "=r"(out) : "r"(t), "r"(i));
    results[k++] = out;
    // Tainted base + tainted index*8 + disp8.
    asm volatile("lea 0x10(%1,%2,8), %0" : "=r"(out) : "r"(t), "r"(i));
    results[k++] = out;
    // Tainted value as index only (no base register): the base-slot
    // tainted value serves as the index, *4.
    asm volatile("lea (,%1,4), %0" : "=r"(out) : "r"(t));
    results[k++] = out;
    // RIP-relative (clean by construction; compare directly).
    asm volatile("leaq buf(%%rip), %0" : "=r"(out));
    if (out != (uint64_t)buf) return 2;
    // Launder the tainted results through memory: unpoison the sink, reload,
    // then branch on the now-clean values.
    magic(2, (void*)results, sizeof(results), 0); // op=unpoison
    if (results[0] != 24) return 3;
    if (results[1] != 24 + 0x40) return 4;
    if (results[2] != 24 - 0x1000) return 5;
    if (results[3] != 24 + 3) return 6;
    if (results[4] != 24 + 24 + 0x10) return 7;
    if (results[5] != 96) return 8;
    write(1, "lea-ok\n", 7);
    // LEA must ignore segment overrides like real hardware: the result is
    // base + index*scale + disp, WITHOUT the fs/gs base. Ensure the guest's
    // fs base is non-zero so the comparison is meaningful, then check that
    // a %fs:-override lea of a tainted base computes the same offset as the
    // plain lea (and does not fault -- an override is still not an access).
    // (Zydis normalizes a %fs:-prefixed LEA's operand to segment DS, so
    // this pins the observable behavior end-to-end -- compute_ea ignores
    // segments by design and the decoder never hands LEA an fs/gs base.)
    uint64_t fsb = 0;
    {
        uint64_t ret;
        asm volatile("syscall"
                     : "=a"(ret)
                     : "0"(158ULL) /* SYS_arch_prctl */,
                       "D"(0x1003ULL) /* ARCH_GET_FS */, "S"((uint64_t)&fsb)
                     : "rcx", "r11", "memory");
        (void)ret;
    }
    static char dummy_fs[64];
    if (fsb == 0) {
        asm volatile("syscall" :: "a"(158ULL), "D"(0x1002ULL) /* ARCH_SET_FS */,
                     "S"((uint64_t)dummy_fs)
                     : "rcx", "r11", "memory");
    }
    uint64_t plain, segged;
    asm volatile("lea 0x10(%1), %0" : "=r"(plain) : "r"(t));
    asm volatile("lea %%fs:0x10(%1), %0" : "=r"(segged) : "r"(t));
    // Both results are tainted (derived from t), so they cannot be compared
    // and branched on directly (cmp taints the flags -- the same wrinkle as
    // above): launder them through the sink, unpoison, reload, then compare
    // the now-clean values (t is known to be 24, matching the checks above).
    results[0] = plain;
    results[1] = segged;
    magic(2, (void*)results, sizeof(results), 0); // op=unpoison
    int ok = (results[0] == 24 + 0x10) && (results[1] == results[0]);
    // Raw exit: glibc's exit path touches TLS, and FS may now be the dummy
    // above -- never return through libc after touching FS.
    asm volatile("syscall" :: "a"(60ULL) /* SYS_exit */, "D"(ok ? 0ULL : 1ULL)
                 : "rcx", "r11", "memory");
    __builtin_unreachable();
}
