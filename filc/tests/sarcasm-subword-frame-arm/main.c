#include <stdio.h>

/* Sub-width accesses on frame slots must behave like real memory: byte/half
   stores then sign/zero-extended reloads of the same offset, mixed-width
   store->load of one slot, odd offsets, and gcc-style add x29, sp, #N frames. */
long subword(long x);   /* strb/strh + ldrsb/ldrb/ldrsh/ldrh on [sp] and [x29] */
long mixwidth(long x);  /* str w then ldrb/ldrh/ldr of the same slot */
long signcheck(long x); /* str w then ldrsw of the same slot */
long fpgcc(long x);     /* add x29, sp, #16 frame, negative/odd x29 offsets */

int main()
{
    /* subword(0xFF80): -128 + 128 + -128 + 65408 + -128 = 65152 */
    printf("%ld\n", subword(0xFF80L));
    /* mixwidth(0xDEADBEEF): 0xEF + 0xBEEF + 0xDEADBEEF = 3735977677 */
    printf("%ld\n", mixwidth(0xDEADBEEFL));
    /* signcheck(0xDEADBEEF): ldrsw sign-extends to -559038737 */
    printf("%ld\n", signcheck(0xDEADBEEFL));
    /* fpgcc(0xFF81): -127 + -127 = -254 */
    printf("%ld\n", fpgcc(0xFF81L));
    return 0;
}
