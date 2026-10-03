#include <stdio.h>

extern long ones(long x);

int main(void)
{
    long r = ones(0);
    if (r != -1) {
        printf("FAIL ones %ld\n", r);
        return 1;
    }
    printf("masked stack ones att ok\n");
    return 0;
}
