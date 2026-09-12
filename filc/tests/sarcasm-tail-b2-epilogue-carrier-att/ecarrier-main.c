#include <stdio.h>

extern long ecar_jump(long a);
extern long ecar_owner(long a);

int main(void)
{
    /* the tail returns its argument through the recovered rsp + teardown */
    if (ecar_jump(99) != 99) {
        printf("FAIL jump %ld\n", ecar_jump(99));
        return 1;
    }
    if (ecar_owner(99) != 99) {
        printf("FAIL owner\n");
        return 1;
    }
    if (ecar_jump(-7) != -7) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 epilogue carrier att ok\n");
    return 0;
}
