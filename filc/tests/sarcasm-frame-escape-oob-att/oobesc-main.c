#include <stdio.h>

extern long oobesc(long x);

void fill32(long *p)
{
    for (int i = 0; i < 4; i++) p[i] = 100 + i;
}

int main(void)
{
    /* The escape cluster promotes the frame to a GC region; the derived
       pointer 128 bytes past the region+16 base is far past the region upper
       bound, so the read must panic before this program produces any output.
       (If the bounds check somehow did not fire, the print below makes the
       failure visible and the exit code wrong for the manifest.) */
    long r = oobesc(0);
    printf("FAIL: oobesc returned %ld without panicking\n", r);
    return 1;
}
