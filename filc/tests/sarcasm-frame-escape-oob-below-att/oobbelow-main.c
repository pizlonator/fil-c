#include <stdio.h>

extern long oobbelow(long x);

void fill32(long *p)
{
    for (int i = 0; i < 4; i++) p[i] = 100 + i;
}

int main(void)
{
    /* The escape cluster promotes the frame to a GC region; the derived
       pointer 128 bytes below the region+16 base is below the region lower
       bound, so the read must panic before this program produces any output.
       (If the bounds check somehow did not fire, the print below makes the
       failure visible and the exit code wrong for the manifest.) */
    long r = oobbelow(0);
    printf("FAIL: oobbelow returned %ld without panicking\n", r);
    return 1;
}
