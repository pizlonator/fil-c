#include <stdio.h>

extern long redesc(long x);

void fill32(long *p)
{
    for (int i = 0; i < 4; i++) p[i] = 100 + i;
}

int main(void)
{
    /* 375 (the red-zone RMW, intact across the call) + 100 + 101 + 101 + 102 +
       103 + 100 = 982. */
    long r = redesc(0);
    if (r != 982) {
        printf("FAIL: got %ld, want 982\n", r);
        return 1;
    }
    printf("frame escape redzone att ok\n");
    return 0;
}
