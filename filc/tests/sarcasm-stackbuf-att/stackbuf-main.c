#include <stdio.h>
#include <string.h>

int sb_basic(unsigned long idx);
int sb_idx_store_static_load(unsigned long idx);
int sb_fp_gpr(void* src, unsigned long idx);
int sb_scale4(unsigned long idx);
int sb_scale8(unsigned long idx);
int sb_overlap(unsigned long idx);
int sb_short_form(unsigned long idx);
int sb_rec(unsigned long n);

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d\n", __FILE__, __LINE__); fails++; } } while (0)

int main() {
    // The user's example: rbx/rdi == 16 reads back the value stored at 16(%rsp).
    CHECK(sb_basic(16) == 0x41424344);
    // Other in-bounds offsets read whatever the buffer holds (unwritten bytes
    // are stack garbage); they must not trap. The exact upper boundary for a
    // 4-byte access (idx = 28) is exercised below via sb_scale4/sb_scale8.

    // Indexed store at offset idx, static read of offset 4.
    CHECK(sb_idx_store_static_load(4) == 4);

    // SIMD store then GPR indexed load of the same bytes.
    {
        unsigned char src[16];
        for (int i = 0; i < 16; i++) src[i] = (unsigned char)(0x40 + i);
        int lo = sb_fp_gpr(src, 0);
        int hi = sb_fp_gpr(src, 8);
        unsigned int want_lo, want_hi;
        __builtin_memcpy(&want_lo, src, 4);
        __builtin_memcpy(&want_hi, src + 8, 4);
        CHECK((unsigned int)lo == want_lo);
        CHECK((unsigned int)hi == want_hi);
    }

    // Scale-4: idx selects a dword.
    CHECK(sb_scale4(0) == 0x11111111);
    CHECK(sb_scale4(1) == 0x22222222);
    CHECK(sb_scale4(2) == 0x33333333);
    CHECK(sb_scale4(3) == 0x44444444);
    // exact upper boundary: idx = hi - size = (64 - 4)/4 = 15
    CHECK(sb_scale4(15) == 0x55555555);

    // Scale-8: idx in [0, 7]; the boundary idx = 7 reads bytes 56..63.
    CHECK(sb_scale8(7) == (int)0x05060708);

    // Overlapping buffers: the two declared ranges merge into one lowered
    // region. Store two dwords (16 and 20 — both inside the union), read them
    // back through o1's indexed form at 16 and o2's static spelling at 20.
    CHECK(sb_overlap(16) == (int)(0xcafebabe ^ 0x0badf00d));

    // Short form (the long form lives in sb_basic).
    CHECK(sb_short_form(12) == 0x56575859);

    // Re-entrancy: each frame's buffer is distinct stack memory.
    {
        int v = sb_rec(4);
        // The innermost call wrote 0 at its buffer+0 and returned it up through
        // each level; every level re-read its own slot after the deeper call
        // returned, and the returned value is the innermost's.
        CHECK(v == 4);  // the outermost frame re-reads its own slot after the deeper call
    }

    if (fails == 0) printf("stack buffer ok\n");
    return fails ? 1 : 0;
}
