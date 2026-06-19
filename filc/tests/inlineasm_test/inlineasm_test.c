#include <stdfil.h>

int main(void)
{
    unsigned long x;
    unsigned long y;
    unsigned char zf;

    /* test with immediate zero: result is 0 so ZF should be set. */
    x = 0x12345678;
    asm volatile("test $0, %1\n\tsetz %0"
                 : "=r"(zf)
                 : "r"(x)
                 : "cc");
    ZASSERT(zf == 1);
    ZASSERT(x == 0x12345678);

    /* test with overlapping bits: result is nonzero so ZF should be clear. */
    x = 0xff;
    asm volatile("test $0x10, %1\n\tsetz %0"
                 : "=r"(zf)
                 : "r"(x)
                 : "cc");
    ZASSERT(zf == 0);

    /* test with non-overlapping bits: result is 0 so ZF should be set. */
    x = 0xf0;
    asm volatile("test $0x0f, %1\n\tsetz %0"
                 : "=r"(zf)
                 : "r"(x)
                 : "cc");
    ZASSERT(zf == 1);

    /* test two registers, overlapping: ZF clear. */
    x = 0xc0;
    y = 0x40;
    asm volatile("test %2, %1\n\tsetz %0"
                 : "=r"(zf)
                 : "r"(x), "r"(y)
                 : "cc");
    ZASSERT(zf == 0);
    ZASSERT(x == 0xc0);
    ZASSERT(y == 0x40);

    /* test two registers, non-overlapping: ZF set. */
    x = 0xc0;
    y = 0x03;
    asm volatile("test %2, %1\n\tsetz %0"
                 : "=r"(zf)
                 : "r"(x), "r"(y)
                 : "cc");
    ZASSERT(zf == 1);

    /* test reg with itself when zero: ZF set. */
    x = 0;
    asm volatile("test %1, %1\n\tsetz %0"
                 : "=r"(zf)
                 : "r"(x)
                 : "cc");
    ZASSERT(zf == 1);

    /* test reg with itself when nonzero: ZF clear. */
    x = 42;
    asm volatile("test %1, %1\n\tsetz %0"
                 : "=r"(zf)
                 : "r"(x)
                 : "cc");
    ZASSERT(zf == 0);

    return 0;
}
