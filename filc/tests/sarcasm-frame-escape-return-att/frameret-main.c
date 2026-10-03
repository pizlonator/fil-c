#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

extern long *frameret(long x);
extern long *frametouch(long *p);

void fill32(long *p)
{
    for (int i = 0; i < 4; i++) p[i] = 100 + i;
}

int main(void)
{
    long *p = frameret(0);
    /* p = the promoted frame's region+16: the two magics, the helper's 102/103,
       and the parked checksum at p[5] (region+56). */
    if (p[0] != 4111 || p[1] != 4222 || p[2] != 102 || p[3] != 103 ||
        p[5] != 8538) {
        printf("FAIL: bad region values %ld %ld %ld %ld %ld\n",
               p[0], p[1], p[2], p[3], p[5]);
        return 1;
    }
    /* GC churn: a non-rooted region would be collected here. */
    for (int i = 0; i < 100000; i++) {
        volatile long *q = malloc(64);
        if (!q)
            return 1;
        *q = (long)i;
    }
    zgc_request_and_wait();
    if (p[0] != 4111 || p[1] != 4222 || p[2] != 102 || p[3] != 103 ||
        p[5] != 8538) {
        printf("FAIL: region values did not survive GC\n");
        return 1;
    }
    p[0] = 20881;               /* write through the returned pointer */
    long *q = frametouch(p);
    /* frametouch read the old region through the kept pointer (20881 + 4222 +
       102 + 103 + 8538 = 33846) and parked that back at p[5]. */
    if (p[5] != 33846) {
        printf("FAIL: frametouch read %ld, want 33846\n", p[5]);
        return 1;
    }
    /* q = the SECOND activation's own fresh region+16. */
    if (q[0] != 29555 || q[1] != 101 || q[2] != 102 || q[3] != 103 ||
        q[5] != 29861) {
        printf("FAIL: fresh region values %ld %ld %ld %ld %ld\n",
               q[0], q[1], q[2], q[3], q[5]);
        return 1;
    }
    printf("frame escape return att ok\n");
    return 0;
}
