#include <stdio.h>

extern long maskedload(long x);

int main(void)
{
    long r = maskedload(0);
    if (r != 0) {
        printf("FAIL maskedload %ld\n", r);
        return 1;
    }
    printf("masked stack load att ok\n");
    return 0;
}
