#include <stdio.h>

extern long d0lea_jump(long x);
extern long d0lea_owner(long x);

int main(void)
{
    if (d0lea_jump(21) != 42) {
        printf("FAIL d0lea jump %ld\n", d0lea_jump(21));
        return 1;
    }
    if (d0lea_owner(21) != -42) {
        printf("FAIL d0lea owner\n");
        return 1;
    }
    if (d0lea_jump(-4) != -8 || d0lea_owner(0) != 0) {
        printf("FAIL d0lea more\n");
        return 1;
    }
    printf("b2clone d0 lea att ok\n");
    return 0;
}
