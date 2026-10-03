#include <stdio.h>

long ud2_midjump(long);

int main(void)
{
    if (ud2_midjump(7) != 7) {
        printf("FAIL ud2 midjump\n");
        return 1;
    }
    printf("ud2 midjump ok\n");
    return 0;
}
