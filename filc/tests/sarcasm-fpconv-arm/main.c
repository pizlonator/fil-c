#include <stdio.h>

/* FP<->GPR conversions in instrumented code: scvtf must read the right GPR
   web, fmov both directions must not swap def/use, fcvtzs writes back the
   converted GPR. */
void conv(double* p, long a, long b);
long roundtrip(long x);
long lanes(long a, long b);

int main()
{
    double p[2];
    conv(p, 7, 42);
    printf("%g %g\n", p[0], p[1]);
    printf("%ld %ld\n", roundtrip(42), roundtrip(-12345));
    /* umov/smov lane extracts: a=0xff80 gives byte1=255 (umov) and low16=-128
       (smov); b=0x80000005 gives lane=2147483653 (umov) and low32=-2147483643
       (smov). 2147483653-2147483643+255-128 = 137. */
    printf("%ld\n", lanes(0xff80L, 0x80000005L));
    return 0;
}
