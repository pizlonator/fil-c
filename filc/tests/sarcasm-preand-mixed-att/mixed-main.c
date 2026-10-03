#include <stdio.h>

extern long preandmix(long x);

int main(void)
{
    if (preandmix(21) != 42) {
        printf("FAIL mixed %ld\n", preandmix(21));
        return 1;
    }
    if (preandmix(-7) != -14) {
        printf("FAIL mixed neg\n");
        return 1;
    }
    printf("preand mixed att ok\n");
    return 0;
}
