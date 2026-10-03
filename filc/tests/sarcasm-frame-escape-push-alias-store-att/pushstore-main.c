#include <stdio.h>
#include <stdlib.h>

extern long pushst(long *bufA, long *bufB, long seed);
extern long pushst2(long *bufA, long *bufB, long seed);

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
    bufA[0] = bufB[0] = -1;

    long r1 = pushst(bufA, bufB, 900);
    if (r1 != 4974) {
        printf("FAIL: pushst got %ld, want 4974\n", r1);
        return 1;
    }
    if (bufA[0] != 102 || bufB[0] != 108) {
        printf("FAIL: pushst bufs %ld %ld, want 102 108\n", bufA[0], bufB[0]);
        return 1;
    }
    bufA[0] = bufB[0] = -1;

    long r2 = pushst2(bufA, bufB, 900);
    if (r2 != 4578) {
        printf("FAIL: pushst2 got %ld, want 4578\n", r2);
        return 1;
    }
    if (bufA[0] != 102 || bufB[0] != 108) {
        printf("FAIL: pushst2 bufs %ld %ld, want 102 108\n", bufA[0], bufB[0]);
        return 1;
    }
    printf("frame escape push alias store att ok\n");
    return 0;
}
