#include <stdio.h>

extern long lead0val(long x);

int main(void)
{
    if (lead0val(21) != 201) {
        printf("FAIL d0 value %ld\n", lead0val(21));
        return 1;
    }
    printf("lea carrier d0 value att ok\n");
    return 0;
}
