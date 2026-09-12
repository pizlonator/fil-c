#include <stdio.h>

extern long cse_jump(long a);
extern long cse_owner(long a);

int main(void)
{
    /* helper adds 1; both paths agree */
    if (cse_jump(41) != 42) {
        printf("FAIL jump %ld\n", cse_jump(41));
        return 1;
    }
    if (cse_owner(41) != 42) {
        printf("FAIL owner\n");
        return 1;
    }
    if (cse_jump(-7) != -6 || cse_owner(0) != 1) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 carrier callee att ok\n");
    return 0;
}
