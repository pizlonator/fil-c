#include <stdio.h>

extern long parkpost(void *buf);

int main(void)
{
    static long buf[4] __attribute__((aligned(32)));
    buf[0] = 17; buf[1] = 17;
    if (parkpost(buf) != 1) {
        printf("FAIL parkpost %ld\n", parkpost(buf));
        return 1;
    }
    printf("parkpost and att ok\n");
    return 0;
}
