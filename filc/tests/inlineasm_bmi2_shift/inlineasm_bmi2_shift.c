#include <stdfil.h>

/* SARX/SHLX/SHRX (BMI2) shift a source register by a count held in a GPR and
 * write the result to a destination register, without affecting flags. AT&T
 * operand order is: count, src, dest. Verify they are accepted and behave
 * correctly. */

static unsigned long ref_shlx(unsigned long src, unsigned count)
{
    return src << (count & 63);
}

static unsigned long ref_shrx(unsigned long src, unsigned count)
{
    return src >> (count & 63);
}

static long ref_sarx(long src, unsigned count)
{
    return src >> (count & 63);
}

int main(void)
{
    unsigned long src = 0xdeadbeefcafebabeUL;

    for (unsigned c = 0; c <= 70; c += 7) {
        unsigned long dshlx, dshrx;
        long dsarx;
        unsigned long cnt = c;
        asm("shlx %2, %1, %0" : "=r"(dshlx) : "r"(src), "r"(cnt));
        asm("shrx %2, %1, %0" : "=r"(dshrx) : "r"(src), "r"(cnt));
        asm("sarx %2, %1, %0" : "=r"(dsarx) : "r"((long)src), "r"(cnt));
        ZASSERT(dshlx == ref_shlx(src, c));
        ZASSERT(dshrx == ref_shrx(src, c));
        ZASSERT(dsarx == ref_sarx((long)src, c));
    }

    /* 32-bit operands. */
    unsigned int s32 = 0x12345678u;
    unsigned int d32;
    unsigned int cnt32 = 4;
    asm("shlxl %2, %1, %0" : "=r"(d32) : "r"(s32), "r"(cnt32));
    ZASSERT(d32 == (s32 << 4));

    asm("shrxl %2, %1, %0" : "=r"(d32) : "r"(s32), "r"(cnt32));
    ZASSERT(d32 == (s32 >> 4));

    asm("sarxl %2, %1, %0" : "=r"(d32) : "r"((int)s32), "r"(cnt32));
    ZASSERT(d32 == (unsigned)((int)s32 >> 4));

    zprintf("bmi2 shift ok\n");
    return 0;
}
