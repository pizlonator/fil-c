#include <stdio.h>

extern long gap_jump(long a, long b);
extern long gap_owner(long a, long b);

/* tail: slot8 = a; rax = slot8 + slot0 = a + b */
static long expect(long a, long b) { return a + b; }

int main(void)
{
    if (gap_jump(100, 7) != expect(100, 7)) {
        printf("FAIL jump %ld\n", gap_jump(100, 7));
        return 1;
    }
    if (gap_owner(100, 7) != expect(100, 7)) {
        printf("FAIL owner\n");
        return 1;
    }
    if (gap_jump(-5, 3) != expect(-5, 3)) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 gap att ok\n");
    return 0;
}
