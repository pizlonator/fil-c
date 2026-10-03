#include <stdio.h>

extern long fb_jmp(long a, long b);
extern long fb_jne(long a, long b);

int main(void)
{
    if (fb_jmp(10, 3) != 23) {
        printf("FAIL jmp ne %ld\n", fb_jmp(10, 3));
        return 1;
    }
    if (fb_jmp(7, 7) != 28) {
        printf("FAIL jmp eq %ld\n", fb_jmp(7, 7));
        return 1;
    }
    if (fb_jne(10, 3) != 23) {
        printf("FAIL jne ne %ld\n", fb_jne(10, 3));
        return 1;
    }
    if (fb_jne(7, 7) != 28) {
        printf("FAIL jne eq %ld\n", fb_jne(7, 7));
        return 1;
    }
    if (fb_jmp(-9, -9) != -36 || fb_jne(-9, 4) != -14) {
        printf("FAIL neg %ld %ld\n", fb_jmp(-9, -9), fb_jne(-9, 4));
        return 1;
    }
    printf("b2 flags int ok\n");
    return 0;
}
