#include <stdio.h>

extern long oan_jump(long a, long b);
extern long oan_owner(long a, long b);

/* tail: slot = a; rax = slot + b */
static long expect(long a, long b) { return a + b; }

int main(void)
{
    if (oan_jump(100, 7) != expect(100, 7)) {
        printf("FAIL jump %ld\n", oan_jump(100, 7));
        return 1;
    }
    if (oan_owner(100, 7) != expect(100, 7)) {
        printf("FAIL owner\n");
        return 1;
    }
    if (oan_jump(-5, 3) != expect(-5, 3)) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 ownand att ok\n");
    return 0;
}
