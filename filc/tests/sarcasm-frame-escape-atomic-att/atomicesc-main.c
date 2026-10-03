#include <stdio.h>

extern long atomicesc(long x);

void seed2(long *p)
{
    p[0] = 100;
    p[1] = 200;
}

/* Pure-C mirror of atomicesc's locked sequence (single-threaded, so the
   outcomes are the plain C semantics). */
static long mirror(void)
{
    long cell[2] = { 100, 200 };
    long *rbx = cell;
    long r10, r8, r9, rax, rcx;
    r10 = rbx[0];               /* lock xaddq 5: old = 100 */
    rbx[0] = r10 + 5;
    rbx[0] += r10;              /* lock addq old: 205 */
    rbx[1] |= 0x0F;             /* 207 */
    rbx[1] &= 0xFF;             /* 207 */
    rbx[1] ^= 200;              /* 7 */
    rax = 205;
    rcx = 0x64;
    if (rbx[0] == rax) {        /* cmpxchg success */
        rbx[0] = rcx;
        r8 = 1;
    } else
        r8 = 0;
    rax = 205;
    rcx = 0x11;
    if (rbx[0] == rax) {        /* cmpxchg failure */
        rbx[0] = rcx;
        r9 = 1;
    } else {
        rax = rbx[0];           /* rax = the loaded 100 */
        r9 = 0;
    }
    rax += cell[0] + cell[1];   /* direct slot reads: 100 + 7 */
    rax += rbx[0] + rbx[1];     /* pointer reads: 100 + 7 */
    rax += r10 + r8 + r9;       /* old value + ZFs: 101 */
    return rax;                 /* 415 */
}

int main(void)
{
    long want = mirror();
    if (want != 415) {
        printf("FAIL: mirror self-check got %ld, want 415\n", want);
        return 1;
    }
    for (int i = 0; i < 2; i++) {
        long r = atomicesc(0);
        if (r != 415) {
            printf("FAIL: got %ld, want 415\n", r);
            return 1;
        }
    }
    printf("frame escape atomic att ok\n");
    return 0;
}
