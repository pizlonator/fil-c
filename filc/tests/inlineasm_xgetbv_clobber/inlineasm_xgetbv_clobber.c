#include <stdfil.h>
#include <stdint.h>

int main(void)
{
    /* XGETBV reads XCR0 (selected by ecx = 0) into edx:eax. The edx half of
       the result is discarded here, so it is declared as a clobber rather
       than an output constraint. This is the shape used by xgetbv.c at the
       repo root (and by glibc's get_xcr0). */
    uint32_t xcr0;
    asm volatile("xgetbv"
                 : "=a"(xcr0)
                 : "c"(0)
                 : "%edx");

    zprintf("xgetbv(0) via edx clobber = 0x%x\n", xcr0);

    /* The x87 in-use bit of XCR0 is always set when OSXSAVE is enabled. */
    ZASSERT((xcr0 & 1) == 1);

    /* The same XCR0 read with the classic two-output shape must agree. */
    uint32_t lo, hi;
    asm volatile("xgetbv"
                 : "=a"(lo), "=d"(hi)
                 : "c"(0));
    zprintf("xgetbv(0) via two outputs = 0x%x%08x\n", hi, lo);
    ZASSERT(lo == xcr0);

    return 0;
}
