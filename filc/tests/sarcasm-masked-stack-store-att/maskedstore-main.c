#include <stdio.h>

extern long maskedstore(long x);

int main(void)
{
    long r = maskedstore(0);
    if (r != -1) {
        printf("FAIL maskedstore %ld\n", r);
        return 1;
    }
    printf("masked stack store att ok\n");
    return 0;
}
