#include <stdio.h>

extern long and512(void *buf);

int main(void)
{
    static long buf[8] __attribute__((aligned(64)));
    for (int i = 0; i < 8; i++) {
        buf[i] = 17;
    }
    if (and512(buf) != 1) {
        printf("FAIL and512\n");
        return 1;
    }
    printf("frame and512 att ok\n");
    return 0;
}
