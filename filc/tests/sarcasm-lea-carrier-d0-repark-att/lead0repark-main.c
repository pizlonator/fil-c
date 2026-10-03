#include <stdio.h>

extern long lead0repark(long x);

int main(void)
{
    if (lead0repark(21) != 42) {
        printf("FAIL d0 repark %ld\n", lead0repark(21));
        return 1;
    }
    if (lead0repark(-9) != -18) {
        printf("FAIL d0 repark neg %ld\n", lead0repark(-9));
        return 1;
    }
    printf("lea carrier d0 repark att ok\n");
    return 0;
}
