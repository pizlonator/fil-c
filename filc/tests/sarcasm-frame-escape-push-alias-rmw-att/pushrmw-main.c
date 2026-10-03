#include <stdio.h>
#include <stdlib.h>

extern long rmwok(long *bufA, long *bufB, long seed);
extern long rmwok2(long *bufA, long *bufB, long seed);

/* The sink every callsite targets. Writes a distinct tag through each of its
   six pointer arguments, stores the SEVENTH (a long — the marshalled pad word
   whose slot is the outstanding push's save slot) through the eighth, and
   returns the tag sum: 231. */
__attribute__((noinline)) long sink8(long *p1, long *p2, long *p3, long *p4,
                                     long *p5, long *p6, long p7, long *p8)
{
    p1[0] = 11;
    p2[1] = 22;
    p3[2] = 33;
    p4[3] = 44;
    p5[4] = 55;
    p6[5] = 66;
    p8[6] = p7;
    return 11 + 22 + 33 + 44 + 55 + 66;
}

static int fail(const char *what, long got, long want)
{
    printf("FAIL: %s got %ld, want %ld\n", what, got, want);
    return 1;
}

int main(void)
{
    long *bufA = malloc(16 * sizeof(long));
    long *bufB = malloc(16 * sizeof(long));
    if (!bufA || !bufB)
        return 1;
    bufA[0] = bufB[0] = -7777;
    bufA[1] = bufB[1] = -7777;

    /* checksums (hardware-exact, verified against a native gcc build of the
       same asm): each function returns the sink's tag sum (231) + the tag
       readbacks (11 + 22 + 33 + 44 + 55 + 66 = 231) + the two pinned 900s of
       the pad word's round trip — the popped web (900+V minus the region
       pointer V for the addq form; the restored pushed value for the notq
       form) and the marshalled outgoing word 0 that the sink stored at
       bufA[6] (re-derived against V inside the asm for the addq form, whose
       marshalled word is the address-dependent 900+V; read back directly for
       the notq form, whose second notq restored the web to the pushed 900
       before the marshal). The sink's writes land in the malloc'd buffers and
       in the promoted region through the register and STACK-PASSED
       arguments. */
    long r1 = rmwok(bufA, bufB, 900);
    if (r1 != 2262)
        return fail("rmwok", r1, 2262);
    if (bufB[0] != 11)
        return fail("bufB[0]", bufB[0], 11);
    if (bufA[1] != 22)
        return fail("bufA[1]", bufA[1], 22);

    long r2 = rmwok2(bufA, bufB, 900);
    if (r2 != 2262)
        return fail("rmwok2", r2, 2262);
    /* the notq form's marshalled pad word is the RESTORED pushed value — an
       absolute 900 on both sides (no address arithmetic to undo): */
    if (bufA[6] != 900)
        return fail("bufA[6] (the marshalled word 0)", bufA[6], 900);
    if (bufB[0] != 11 || bufA[1] != 22)
        return fail("second-call tags", bufB[0] * 1000 + bufA[1], 11022);

    printf("frame escape push alias rmw att ok\n");
    return 0;
}
