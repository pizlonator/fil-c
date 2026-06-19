#include <stdfil.h>

/* SHLD/SHRD are double-precision shifts: they shift the destination (read and
 * written) while shifting bits in from a second source register. The count is
 * an imm8 or CL. AT&T operand order is: count, src, dest. */

static unsigned long ref_shld(unsigned long dest, unsigned long src,
                              unsigned count)
{
    unsigned c = count & 63;
    if (c == 0)
        return dest;
    return (dest << c) | (src >> (64 - c));
}

static unsigned long ref_shrd(unsigned long dest, unsigned long src,
                              unsigned count)
{
    unsigned c = count & 63;
    if (c == 0)
        return dest;
    return (dest >> c) | (src << (64 - c));
}

#define CHECK_IMM(c) do { \
    unsigned long _d1 = dest, _d2 = dest; \
    asm("shldq $" #c ", %1, %0" : "+r"(_d1) : "r"(src) : "cc"); \
    asm("shrdq $" #c ", %1, %0" : "+r"(_d2) : "r"(src) : "cc"); \
    ZASSERT(_d1 == ref_shld(dest, src, (c))); \
    ZASSERT(_d2 == ref_shrd(dest, src, (c))); \
} while (0)

int main(void)
{
    unsigned long dest = 0x123456789abcdef0UL;
    unsigned long src = 0x0fedcba987654321UL;

    /* Immediate count (a few fixed values). */
    CHECK_IMM(0);
    CHECK_IMM(4);
    CHECK_IMM(8);
    CHECK_IMM(16);
    CHECK_IMM(32);
    CHECK_IMM(60);

    /* CL count via "c" input constraint. */
    for (unsigned c = 0; c <= 70; c += 7) {
        unsigned long d1 = dest, d2 = dest;
        unsigned char cnt = (unsigned char)c;
        asm("shldq %2, %1, %0" : "+r"(d1) : "r"(src), "c"(cnt) : "cc");
        asm("shrdq %2, %1, %0" : "+r"(d2) : "r"(src), "c"(cnt) : "cc");
        ZASSERT(d1 == ref_shld(dest, src, c));
        ZASSERT(d2 == ref_shrd(dest, src, c));
    }

    /* 32-bit operands. */
    {
        unsigned int d32 = 0x12345678u;
        unsigned int s32 = 0x9abcdef0u;
        unsigned int before = d32;
        asm("shldl $4, %1, %0" : "+r"(d32) : "r"(s32) : "cc");
        ZASSERT(d32 == ((before << 4) | (s32 >> 28)));
    }
    {
        unsigned int d32 = 0x12345678u;
        unsigned int s32 = 0x9abcdef0u;
        unsigned int before = d32;
        asm("shrdl $4, %1, %0" : "+r"(d32) : "r"(s32) : "cc");
        ZASSERT(d32 == ((before >> 4) | (s32 << 28)));
    }

    zprintf("shld/shrd ok\n");
    return 0;
}
