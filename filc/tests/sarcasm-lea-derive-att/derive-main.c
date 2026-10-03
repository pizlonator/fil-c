#include <stdio.h>

extern long leaderive(long x, long y);

int main(void)
{
    if (leaderive(20, 22) != 62) {
        printf("FAIL derive %ld\n", leaderive(20, 22));
        return 1;
    }
    if (leaderive(-5, 9) != -1) {
        printf("FAIL derive neg\n");
        return 1;
    }
    printf("lea derive att ok\n");
    return 0;
}
