#include <stdio.h>
#include <string.h>

extern long depthesc(long x);

void stomp8(long *p)
{
    for (int i = 0; i < 4; i++) p[i] = 256 + i;
}

/* Pure-C mirror of depthesc's asm sequence, slot for slot. R models the
   promoted frame's region (one GC allocation; R[k] is the slot at offset
   8*k). The spill-push slots live below the region base (the hardware red
   zone), so they round-trip through plain variables. */
static long mirror(void)
{
    long R[16];
    long *rdi, *rsi, *rdx;
    long rax, rcx, rdxv, acc;
    memset(R, 0, sizeof R);
    rdi = &R[2];                    /* leaq 16(%rsp): region+16 */
    rsi = rdi;                      /* movq %rdi,%rsi */
    rdx = rsi + 1;                  /* leaq 8(%rsi): region+24 */
    *rdi = 10;
    *rsi = 11;                      /* slot16 = 11 */
    *rdx = 12;                      /* slot24 = 12 */
    stomp8(rdi);                    /* slots 16..40 = 256..259 */
    R[4] = 22;                      /* slot32 = 22 */
    R[5] = 23;                      /* slot40 = 23 */
    R[6] = 24;                      /* slot48 = 24 */
    rax = R[4] + R[5];              /* 45 */
    acc = rax;
    /* first depth shift (+8): shifted spellings key disp + D0 - d */
    long parked1 = rax;             /* pushq %rax (slot below the region) */
    R[4] = 33;                      /* 40(%rsp) at depth+8 -> slot32 */
    R[5] = 34;                      /* 48(%rsp) at depth+8 -> slot40 */
    R[6] = 35;                      /* 56(%rsp) at depth+8 -> slot48 */
    R[2] = 13;                      /* 24(%rsp) at depth+8 -> slot16 */
    rcx = R[4] + R[5] + R[6];       /* 102 */
    acc += rcx;                     /* 147 */
    R[5] = 25;                      /* -96(%rbp) -> slot40 */
    rcx = R[6];                     /* -88(%rbp) -> slot48 = 35 */
    acc += rcx;                     /* 182 */
    rcx = R[5];                     /* 48(%rsp) at depth+8 -> slot40 = 25 */
    acc += rcx;                     /* 207 */
    rax = parked1;                  /* popq %rax: the pushed 45 */
    rcx = R[4];                     /* 32(%rsp) at D0 -> slot32 = 33 */
    acc += rcx;                     /* 240 */
    acc += rax;                     /* 285 */
    /* second depth shift (two nested pushes, +16 total) */
    rcx = 77;
    long parked2 = rcx, parked3 = rcx;
    R[5] = 44;                      /* 56(%rsp) at depth+16 -> slot40 */
    R[6] = 45;                      /* 64(%rsp) at depth+16 -> slot48 */
    R[2] = 14;                      /* 32(%rsp) at depth+16 -> slot16 */
    R[4] = 26;                      /* -104(%rbp) -> slot32 */
    rdxv = R[5] + R[6] + R[2] + R[4]; /* 129 */
    acc += rdxv;                    /* 414 */
    rcx = parked3;
    rcx = parked2;                  /* popq %rcx; popq %rcx -> 77 */
    acc += rcx;                     /* 491 */
    /* final readback at the frame-base depth */
    acc += R[2];                    /* 14  -> 505 */
    acc += R[3];                    /* 257 -> 762 */
    acc += R[4];                    /* 26  -> 788 */
    acc += R[5];                    /* 44  -> 832 */
    acc += R[6];                    /* 45  -> 877 */
    R[9] = 51;                      /* -64(%rbp) -> slot72 */
    acc += R[9];                    /* 72(%rsp) read -> 928 */
    acc += R[9];                    /* -64(%rbp) read -> 979 */
    return acc;
}

int main(void)
{
    long want = mirror();
    if (want != 979) {
        printf("FAIL: mirror self-check got %ld, want 979\n", want);
        return 1;
    }
    for (int i = 0; i < 2; i++) {
        long r = depthesc(0);
        if (r != 979) {
            printf("FAIL: got %ld, want 979\n", r);
            return 1;
        }
    }
    printf("frame escape depth att ok\n");
    return 0;
}
