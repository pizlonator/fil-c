#include <stdio.h>

extern long cslot_jump(long a);
extern long cslot_owner(long a);

int main(void)
{
    /* helper adds 1; the slot carrier must survive the call on both paths */
    if (cslot_jump(41) != 42) {
        printf("FAIL jump %ld\n", cslot_jump(41));
        return 1;
    }
    if (cslot_owner(41) != 42) {
        printf("FAIL owner\n");
        return 1;
    }
    if (cslot_jump(-1) != 0) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 carrier slot att ok\n");
    return 0;
}
