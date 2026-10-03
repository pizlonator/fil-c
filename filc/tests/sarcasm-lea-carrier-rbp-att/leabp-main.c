#include <stdio.h>

extern long leabp(long x);

int main(void)
{
    if (leabp(21) != 42) {
        printf("FAIL rbp %ld\n", leabp(21));
        return 1;
    }
    if (leabp(-9) != -18) {
        printf("FAIL rbp neg\n");
        return 1;
    }
    printf("lea carrier rbp att ok\n");
    return 0;
}
