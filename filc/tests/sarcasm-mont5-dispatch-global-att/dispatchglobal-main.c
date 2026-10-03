#include <stdio.h>

/* The asm bodies read the multiplier in %rsi and the addend in %rdx; the
   first slot (%rdi) is unused. */
extern long mg_top(long sel, long a, long b);
extern long mg_top4(long sel, long a, long b);
extern long mg_top_jne(long sel, long a, long b);

int main(void)
{
    /* global word == 0x80108: clone's je taken -> x4x body */
    if (mg_top(0, 10, 3) != 1083) {
        printf("FAIL taken %ld\n", mg_top(0, 10, 3));
        return 1;
    }
    /* global word == 0: clone's je not taken -> 4x body */
    if (mg_top4(0, 10, 3) != 43) {
        printf("FAIL nottaken %ld\n", mg_top4(0, 10, 3));
        return 1;
    }
    /* first-level jne (not taken) then inline je dispatch (taken) -> x4x */
    if (mg_top_jne(0, 10, 3) != 1083) {
        printf("FAIL jne %ld\n", mg_top_jne(0, 10, 3));
        return 1;
    }
    /* negatives */
    if (mg_top(0, -6, 11) != 963 || mg_top4(0, -6, 11) != -13) {
        printf("FAIL neg %ld %ld\n", mg_top(0, -6, 11), mg_top4(0, -6, 11));
        return 1;
    }
    printf("mont5 dispatch global att ok\n");
    return 0;
}
