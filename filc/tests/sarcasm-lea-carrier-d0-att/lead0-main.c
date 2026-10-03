#include <stdio.h>

extern long lead0(long x);

int main(void)
{
    if (lead0(21) != 47) {
        printf("FAIL d0 %ld\n", lead0(21));
        return 1;
    }
    if (lead0(-4) != -3) {
        printf("FAIL d0 neg %ld\n", lead0(-4));
        return 1;
    }
    if (lead0(0) != 5) {
        printf("FAIL d0 zero %ld\n", lead0(0));
        return 1;
    }
    printf("lea carrier d0 att ok\n");
    return 0;
}
