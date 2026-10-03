#include <stdio.h>
#include <stdlib.h>

extern long yoloesc(long *bufA, long *bufB, long seed);

/* The sink every callsite targets. Writes a distinct tag through each of its
   eight pointer arguments (two of which arrive as SysV STACK arguments — the
   region pointer and a malloc'd buffer) and returns their sum: 836. */
__attribute__((noinline)) long sink8(long *p1, long *p2, long *p3, long *p4,
                                     long *p5, long *p6, long *p7, long *p8)
{
    p1[0] = 101;
    p2[0] = 102;
    p3[0] = 103;
    p4[0] = 104;
    p5[0] = 105;
    p6[0] = 106;
    p7[0] = 107;
    p8[0] = 108;
    return 101 + 102 + 103 + 104 + 105 + 106 + 107 + 108;
}

int main(void)
{
    long *bufA = malloc(4 * sizeof(long));
    long *bufB = malloc(4 * sizeof(long));
    if (!bufA || !bufB)
        return 1;
    bufA[0] = 0;
    bufB[0] = 0;
    bufA[1] = -7777;
    bufB[1] = -7777;

    /* checksum (hardware-exact, verified against a native gcc build of the
       same asm): the first call's readback (836 + 103 + 103 + 104 + 105 + 106
       + 107 + 940 + 13 + 14 + 964 + 972 = 4367) + the second call's readback
       (836 + 107 + 107 + 104 + 105 + 106 + 107 + 940 + 13 + 14 + 964 + 972 =
       4375 -- the second call's p7 IS the region pointer stored at the shared
       slot, so its 107 write lands on region+0, clobbering p3's 103) + the
       seed once: 4367 + 4375 + 900 = 9642. */
    long r = yoloesc(bufA, bufB, 900);
    if (r != 9642) {
        printf("FAIL: got %ld, want 9642\n", r);
        return 1;
    }
    /* the sink's writes landed in the malloc'd buffers through the register
       and the STACK-PASSED arguments (the last write to each wins: bufA's is
       the second call's p8, bufB's the second call's p1): */
    if (bufA[0] != 108 || bufB[0] != 101) {
        printf("FAIL: bufs %ld %ld, want 108 101\n", bufA[0], bufB[0]);
        return 1;
    }
    if (bufA[1] != -7777 || bufB[1] != -7777) {
        printf("FAIL: buf overrun\n");
        return 1;
    }
    printf("frame escape push alias att ok\n");
    return 0;
}
