#include <stdio.h>

extern long b2_jump(long x);
extern long b2_owner(long x);

int main(void)
{
    if (b2_jump(21) != 42) {
        printf("FAIL b2 jump %ld\n", b2_jump(21));
        return 1;
    }
    if (b2_owner(21) != 42) {
        printf("FAIL b2 owner\n");
        return 1;
    }
    if (b2_jump(-4) != -8 || b2_owner(0) != 0) {
        printf("FAIL b2 more\n");
        return 1;
    }
    printf("lea carrier b2tail att ok\n");
    return 0;
}
