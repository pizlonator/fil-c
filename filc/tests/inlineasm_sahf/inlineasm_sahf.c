#include <stdfil.h>

int main(void)
{
    unsigned long ax;
    unsigned long x;

    /* SAHF with CF=1 (AH bit 0 = 1), then adc $0 adds CF to x. */
    ax = 0x01 << 8; /* AH = 0x01 */
    x = 10;
    asm volatile("sahf\n\t"
                 "adc $0, %0"
                 : "+r"(x)
                 : "a"(ax)
                 : "cc");
    ZASSERT(x == 11);

    /* SAHF with CF=0 (AH = 0), then adc $0 does not add. */
    ax = 0;
    x = 10;
    asm volatile("sahf\n\t"
                 "adc $0, %0"
                 : "+r"(x)
                 : "a"(ax)
                 : "cc");
    ZASSERT(x == 10);

    /* Round-trip: SAHF loads flags from AH, LAHF reads them back. The flag
       bits (SF, ZF, AF, PF, CF = bits 7,6,4,2,0) should be preserved. */
    unsigned short ax_val;

    /* ZF=1 (AH bit 6). */
    ax_val = 0x40 << 8;
    asm volatile("sahf\n\t"
                 "lahf"
                 : "+a"(ax_val)
                 :
                 : "cc");
    unsigned char ah = ax_val >> 8;
    ZASSERT((ah & 0x40) == 0x40); /* ZF */
    ZASSERT((ah & 0x01) == 0);    /* CF */

    /* CF=1 (AH bit 0). */
    ax_val = 0x01 << 8;
    asm volatile("sahf\n\t"
                 "lahf"
                 : "+a"(ax_val)
                 :
                 : "cc");
    ah = ax_val >> 8;
    ZASSERT((ah & 0x01) == 0x01); /* CF */
    ZASSERT((ah & 0x40) == 0);    /* ZF */

    /* SF=1 (AH bit 7). */
    ax_val = 0x80 << 8;
    asm volatile("sahf\n\t"
                 "lahf"
                 : "+a"(ax_val)
                 :
                 : "cc");
    ah = ax_val >> 8;
    ZASSERT((ah & 0x80) == 0x80); /* SF */

    /* PF=1 (AH bit 2). */
    ax_val = 0x04 << 8;
    asm volatile("sahf\n\t"
                 "lahf"
                 : "+a"(ax_val)
                 :
                 : "cc");
    ah = ax_val >> 8;
    ZASSERT((ah & 0x04) == 0x04); /* PF */

    /* AF=1 (AH bit 4). */
    ax_val = 0x10 << 8;
    asm volatile("sahf\n\t"
                 "lahf"
                 : "+a"(ax_val)
                 :
                 : "cc");
    ah = ax_val >> 8;
    ZASSERT((ah & 0x10) == 0x10); /* AF */

    return 0;
}
