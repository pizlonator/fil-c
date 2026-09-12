#include <stdio.h>

extern long narr_jump(long a, long b);
extern long narr_owner(long a, long b);

/* (b & 0xff) + (b & 0xffff) + a */
static long expect(long a, long b)
{
    return (b & 0xffL) + (b & 0xffffL) + a;
}

int main(void)
{
    if (narr_jump(100, 0x1234) != expect(100, 0x1234)) {
        printf("FAIL jump %ld\n", narr_jump(100, 0x1234));
        return 1;
    }
    if (narr_owner(100, 0x1234) != expect(100, 0x1234)) {
        printf("FAIL owner\n");
        return 1;
    }
    if (narr_jump(-5, 0xAB) != expect(-5, 0xAB)) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 narrow att ok\n");
    return 0;
}
