#include <stdio.h>

long ud2_loop_deadbreak(long);

int main(void)
{
    if (ud2_loop_deadbreak(0) != 0) {
        printf("FAIL ud2 loop dead break\n");
        return 1;
    }
    printf("ud2 loop dead break ok\n");
    return 0;
}
