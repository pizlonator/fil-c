#include <stdio.h>

extern long repark(long x);

int main(void)
{
    if (repark(3) != 10) {
        printf("FAIL repark %ld\n", repark(3));
        return 1;
    }
    if (repark(-2) != -5) {
        printf("FAIL repark neg\n");
        return 1;
    }
    printf("lea carrier repark att ok\n");
    return 0;
}
