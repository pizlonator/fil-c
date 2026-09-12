#include <stdio.h>

extern long goob_jump(long a);

int main(void)
{
    /* Must trap: 8-byte global read 64 bytes past a 12-byte object. */
    long r = goob_jump(1);
    printf("FAIL: survived OOB global read %ld\n", r);
    return 1;
}
