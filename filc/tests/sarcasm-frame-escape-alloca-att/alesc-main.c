#include <stdio.h>

extern long alesc(long arg);

/* Receives the asm's `.alloca` side buffer (arg1) and a derived pointer into
   its promoted frame region (arg2). Reads the asm's patterns back, writes
   through both pointers, and returns a sum: 1000 + 1808 + 7000 + 8000. */
__attribute__((noinline)) long alhelper(long *a, long *b)
{
    long pre_a = a[0] + a[1] + a[2] + a[3];     /* 100+200+300+400 = 1000 */
    long pre_b = b[0] + b[1] + b[2];            /* 901 + 903 + 4 = 1808 */
    a[0] = 7000;                                /* write through the alloca buffer */
    b[0] = 8000;                                /* write through the frame region */
    return pre_a + pre_b + a[0] + b[0];
}

int main(void)
{
    /* checksum: the helper's return (17808) + the region readback (8000 +
       8000 + 903 + 903 + 4 + 5 = 17815) + the alloca readback (7000 + 200 +
       300 + 400 + 7000 = 14900) + the argument (900) = 51423. */
    long r = alesc(900);
    if (r != 51423) {
        printf("FAIL: got %ld, want 51423\n", r);
        return 1;
    }
    printf("frame escape alloca ok\n");
    return 0;
}
