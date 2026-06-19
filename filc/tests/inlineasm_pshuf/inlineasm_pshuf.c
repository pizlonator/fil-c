#include <stdfil.h>

typedef unsigned char v16uqi __attribute__((vector_size(16)));
typedef int v4si __attribute__((vector_size(16)));
typedef short v8hi __attribute__((vector_size(16)));
typedef short v4hi_mmx __attribute__((vector_size(8)));

int main(void)
{
    /* PSHUFB (SSSE3): shuffle bytes in dest according to mask in src.
       result[i] = (mask[i] & 0x80) ? 0 : dest_orig[mask[i] & 0x0f] */
    {
        v16uqi data = { 10, 11, 12, 13, 14, 15, 16, 17,
                        18, 19, 20, 21, 22, 23, 24, 25 };
        v16uqi mask = { 0, 2, 4, 6, 8, 10, 12, 14,
                        1, 3, 5, 7, 9, 11, 13, 15 };
        asm volatile("pshufb %1, %0" : "+x"(data) : "x"(mask));
        ZASSERT(data[0] == 10);
        ZASSERT(data[1] == 12);
        ZASSERT(data[2] == 14);
        ZASSERT(data[3] == 16);
        ZASSERT(data[4] == 18);
        ZASSERT(data[5] == 20);
        ZASSERT(data[6] == 22);
        ZASSERT(data[7] == 24);
        ZASSERT(data[8] == 11);
        ZASSERT(data[9] == 13);
        ZASSERT(data[10] == 15);
        ZASSERT(data[11] == 17);
        ZASSERT(data[12] == 19);
        ZASSERT(data[13] == 21);
        ZASSERT(data[14] == 23);
        ZASSERT(data[15] == 25);
    }

    /* PSHUFD (SSE2): shuffle the 4 dwords of src by imm8 into dst.
       imm8 0x1b = 0b00_01_10_11 reverses the dword order. */
    {
        v4si src = { 100, 200, 300, 400 };
        v4si dst;
        asm volatile("pshufd $0x1b, %1, %0" : "=x"(dst) : "x"(src));
        ZASSERT(dst[0] == 400);
        ZASSERT(dst[1] == 300);
        ZASSERT(dst[2] == 200);
        ZASSERT(dst[3] == 100);
    }

    /* PSHUFHW (SSE2): shuffle the 4 high words of src by imm8 into dst;
       the low 4 words are copied unchanged. */
    {
        v8hi src = { 0, 0, 0, 0, 10, 11, 12, 13 };
        v8hi dst;
        asm volatile("pshufhw $0x1b, %1, %0" : "=x"(dst) : "x"(src));
        ZASSERT(dst[0] == 0);
        ZASSERT(dst[1] == 0);
        ZASSERT(dst[2] == 0);
        ZASSERT(dst[3] == 0);
        ZASSERT(dst[4] == 13);
        ZASSERT(dst[5] == 12);
        ZASSERT(dst[6] == 11);
        ZASSERT(dst[7] == 10);
    }

    /* PSHUFLW (SSE2): shuffle the 4 low words of src by imm8 into dst;
       the high 4 words are copied unchanged. */
    {
        v8hi src = { 10, 11, 12, 13, 0, 0, 0, 0 };
        v8hi dst;
        asm volatile("pshuflw $0x1b, %1, %0" : "=x"(dst) : "x"(src));
        ZASSERT(dst[0] == 13);
        ZASSERT(dst[1] == 12);
        ZASSERT(dst[2] == 11);
        ZASSERT(dst[3] == 10);
        ZASSERT(dst[4] == 0);
        ZASSERT(dst[5] == 0);
        ZASSERT(dst[6] == 0);
        ZASSERT(dst[7] == 0);
    }

    /* PSHUFW (MMX): shuffle the 4 words of a 64-bit MMX register by imm8. */
    {
        v4hi_mmx src = { 10, 11, 12, 13 };
        v4hi_mmx dst;
        asm volatile("pshufw $0x1b, %1, %0" : "=y"(dst) : "y"(src));
        ZASSERT(dst[0] == 13);
        ZASSERT(dst[1] == 12);
        ZASSERT(dst[2] == 11);
        ZASSERT(dst[3] == 10);
    }

    return 0;
}
