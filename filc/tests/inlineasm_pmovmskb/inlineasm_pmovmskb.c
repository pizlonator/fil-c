#include <stdfil.h>

typedef unsigned char v16uqi __attribute__((vector_size(16)));

int main(void)
{
    /* PMOVMSKB: pack the MSB of each byte of an xmm into a 16-bit GPR mask. */
    {
        v16uqi v = { 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
                     0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01 };
        int r;
        asm volatile("pmovmskb %1, %0" : "=r"(r) : "x"(v));
        /* Bits 0,2,4,6,8,10,12,14 set (bytes with 0x80 have MSB=1). */
        ZASSERT(r == 0x5555);
    }

    /* All bytes negative (MSB set) -> all 16 bits set. */
    {
        v16uqi v = { 0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
                     0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff };
        int r;
        asm volatile("pmovmskb %1, %0" : "=r"(r) : "x"(v));
        ZASSERT(r == 0xffff);
    }

    /* No byte has MSB set -> mask is zero. */
    {
        v16uqi v = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                     0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
        int r;
        asm volatile("pmovmskb %1, %0" : "=r"(r) : "x"(v));
        ZASSERT(r == 0x0000);
    }

    /* Single high byte set in position 15. */
    {
        v16uqi v = { 0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0x80 };
        int r;
        asm volatile("pmovmskb %1, %0" : "=r"(r) : "x"(v));
        ZASSERT(r == 0x8000);
    }

    return 0;
}
