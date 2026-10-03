#include <stdio.h>

extern long preand(long x);

int main(void)
{
    if (preand(41) != 42) {
        printf("FAIL preand %ld\n", preand(41));
        return 1;
    }
    if (preand(-2) != -1) {
        printf("FAIL preand neg\n");
        return 1;
    }
    printf("preand rsp att ok\n");
    return 0;
}
