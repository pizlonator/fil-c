#include <stdio.h>
#include <string.h>

extern long fpesc(long x);

void fpfill(double *p)
{
    p[0] = 1.5;
    p[1] = 2.5;
}

/* Pure-C mirror of fpesc's asm sequence. R models the promoted frame's
   region (one GC allocation; R[k] is the slot at offset 8*k). */
static long mirror(void)
{
    long R[16];
    long *rbx, *rdx;
    long acc = 0;
    double d1, d2;
    memset(R, 0, sizeof R);
    fpfill((double *)&R[12]);       /* slots 96/104 = bits(1.5)/bits(2.5) */
    rbx = &R[6];                    /* leaq 48(%rsp): region+48 */
    R[6] = 5;                       /* movdqa {5,5} direct */
    R[7] = 5;
    R[6] = 6;                       /* movaps {6,9} through the pointer */
    R[7] = 9;
    R[8] = 10;                      /* movaps {10,11} at 16(%rbx) */
    R[9] = 11;
    R[7] = 9;                       /* movsd partial-width: slot56 only */
    R[10] = 5;                      /* movdqa {5,5} at 80(%rsp) */
    R[11] = 5;
    acc += R[6] + R[7];             /* direct GPR reads: 15 */
    acc += R[6] + R[7];             /* direct vector readback: 15 */
    acc += R[6] + R[7];             /* movaps through the pointer: 15 */
    acc += R[7];                    /* movsd through the pointer: 9 */
    acc += R[8] + R[9];             /* movaps home via pointer: 21 */
    acc += R[8] + R[9];             /* slots 64/72 direct: 21 */
    acc += R[10] + R[11];           /* (a): 10 */
    memcpy(&d1, &R[12], 8);
    memcpy(&d2, &R[13], 8);
    acc += (long)d1 + (long)d2;     /* cvttsd2si: 1 + 2 */
    rdx = &R[12];
    memcpy(&d1, rdx, 8);
    memcpy(&d2, rdx + 1, 8);
    acc += (long)d1 + (long)d2;     /* 1 + 2 through the pointer */
    return acc;                     /* 112 */
}

int main(void)
{
    long want = mirror();
    if (want != 112) {
        printf("FAIL: mirror self-check got %ld, want 112\n", want);
        return 1;
    }
    for (int i = 0; i < 2; i++) {
        long r = fpesc(0);
        if (r != 112) {
            printf("FAIL: got %ld, want 112\n", r);
            return 1;
        }
    }
    printf("frame escape fp att ok\n");
    return 0;
}
