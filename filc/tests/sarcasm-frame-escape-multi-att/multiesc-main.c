#include <stdio.h>
#include <string.h>

extern long multiesc(long x);

void fillA(long *p)
{
    for (int i = 0; i < 8; i++) p[i] = 100 + i;
}

void fillB(long *p)
{
    for (int i = 0; i < 4; i++) p[i] = 200 + i;
}

/* Pure-C mirror of multiesc's asm sequence. R models the promoted frame's
   region (one GC allocation; R[k] is the slot at offset 8*k). Both clusters'
   derived pointers stay live across both helper calls, exactly like the
   callee-saved parking in the asm. */
static long mirror(void)
{
    long R[16];
    long *r12, *r13, *rbx, *r14;
    long rcx;
    memset(R, 0, sizeof R);
    r12 = &R[2];                    /* cluster A base: region+16 */
    r13 = r12 + 2;                  /* leaq 16(%r12): region+32 */
    rbx = r12 + 3;                  /* leaq 24(%r12): region+40 */
    r12[0] = 1;                     /* slot16 = 1 */
    r12[1] = 2;                     /* slot24 = 2 (through the rdx copy) */
    r14 = &R[10];                   /* cluster B base: region+80 */
    r14[0] = 3;                     /* slot80 = 3 */
    fillA(r12);                     /* slots 16..72 = 100..107 */
    fillB(r14);                     /* slots 80..104 = 200..203 */
    r12[0] = 6;                     /* slot16 = 6 */
    r13[0] = 4;                     /* slot32 = 4 */
    rbx[0] = 5;                     /* slot40 = 5 */
    r14[2] = 7;                     /* slot96 = 7 */
    long rax = R[2] + R[3] + R[4] + R[5] + R[6] + R[7] + R[8] + R[9];
    rcx = R[10] + R[11] + R[12] + R[13];
    rax += rcx;                     /* 1149 */
    rcx = r12[0] + r12[2] + r13[0] + rbx[0] + r14[0] + r14[2];
    rax += rcx;                     /* +226 = 1375 */
    return rax;
}

int main(void)
{
    long want = mirror();
    if (want != 1375) {
        printf("FAIL: mirror self-check got %ld, want 1375\n", want);
        return 1;
    }
    for (int i = 0; i < 2; i++) {
        long r = multiesc(0);
        if (r != 1375) {
            printf("FAIL: got %ld, want 1375\n", r);
            return 1;
        }
    }
    printf("frame escape multi att ok\n");
    return 0;
}
