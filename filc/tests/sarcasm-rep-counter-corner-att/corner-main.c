/* Corner-case main for the rep-string / implicit-counter interaction. */
#include <stdio.h>
#include <string.h>

long fill_seed(void *dst, void *src, long n);
long one_step(void *dst, void *src);
long countdown(void *dst, void *src);

static unsigned long T(unsigned long j)
{
    return j * 0x9E3779B97F4A7C15UL + 0x123456789ABCDEFUL;
}

int main()
{
    unsigned char src[256], dst[256], dst2[256], dst3[256];
    for (int i = 0; i < 256; i++) {
        src[i] = (unsigned char)(i * 7 + 3);
        dst[i] = dst2[i] = dst3[i] = (unsigned char)0xAA;
    }
    long n = 4;

    /* fill_seed: warm loop reads table qwords 16i and 16i+4 (i = 0..7);
       copies n qwords; fills n qwords of n past them; adds accent(3) = 10,
       the copied qword, the first fill qword, and n again. */
    unsigned long warm = 0;
    for (unsigned long i = 0; i < 8; i++) warm ^= T(16 * i) ^ T(16 * i + 4);
    unsigned long q0;
    memcpy(&q0, src, 8);
    unsigned long expect = warm + 10 + q0 + (unsigned long)n + (unsigned long)n;
    unsigned long got = fill_seed(dst, src, n);
    if (got != expect) { printf("BAD fill_seed %lu != %lu\n", got, expect); return 1; }
    if (memcmp(dst, src, 8 * n) != 0) { printf("BAD fill_seed copy\n"); return 1; }
    for (long k = 0; k < n; k++) {
        unsigned long fq;
        memcpy(&fq, dst + 8 * n + 8 * k, 8);
        if (fq != (unsigned long)n) { printf("BAD fill qword %ld\n", k); return 1; }
    }

    /* one_step: warm loop reads table qword 16i (i = 0..7); one movsq copies
       a qword; adds accent(4) = 11 and the copied qword. */
    unsigned long warm2 = 0;
    for (unsigned long i = 0; i < 8; i++) warm2 ^= T(16 * i);
    unsigned long expect2 = warm2 + 11 + q0;
    unsigned long got2 = one_step(dst2, src);
    if (got2 != expect2) { printf("BAD one_step %lu != %lu\n", got2, expect2); return 1; }
    if (memcmp(dst2, src, 8) != 0) { printf("BAD one_step copy\n"); return 1; }

    /* countdown: 3 iterations of qword copy + xor, with a rep movsb stepping
       one byte inside the loop; the countdown must run exactly 3 times. */
    unsigned long q1, q2;
    memcpy(&q1, src + 8, 8);
    memcpy(&q2, src + 16, 8);
    unsigned long expect3 = q0 ^ q1 ^ q2;
    expect3 += 11;
    unsigned long got3 = countdown(dst3, src);
    if (got3 != expect3) { printf("BAD countdown %lu != %lu\n", got3, expect3); return 1; }
    if (memcmp(dst3, src, 24) != 0) { printf("BAD countdown copy\n"); return 1; }

    printf("rep counter corner ok\n");
    return 0;
}
