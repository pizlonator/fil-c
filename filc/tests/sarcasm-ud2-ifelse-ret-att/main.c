#include <stdio.h>

long ud2_ifelse_ret(long);

int main(void)
{
    if (ud2_ifelse_ret(42) != 42) {
        printf("FAIL ud2 ifelse ret\n");
        return 1;
    }
    printf("ud2 ifelse ret ok\n");
    return 0;
}
