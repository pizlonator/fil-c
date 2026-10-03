#include <stdio.h>

extern long and512anchor(void *buf);

int main(void)
{
    static long buf[8] __attribute__((aligned(64)));
    for (int i = 0; i < 8; i++) {
        buf[i] = 17;
    }
    if (and512anchor(buf) != 1) {
        printf("FAIL and512 anchor\n");
        return 1;
    }
    printf("frame and512 anchor att ok\n");
    return 0;
}
