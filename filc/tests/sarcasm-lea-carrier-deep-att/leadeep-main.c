#include <stdio.h>

extern long leadeep(long x);

int main(void)
{
    if (leadeep(21) != 42) {
        printf("FAIL deep %ld\n", leadeep(21));
        return 1;
    }
    if (leadeep(-5) != -10) {
        printf("FAIL deep neg\n");
        return 1;
    }
    if (leadeep(0) != 0) {
        printf("FAIL deep zero\n");
        return 1;
    }
    printf("lea carrier deep att ok\n");
    return 0;
}
