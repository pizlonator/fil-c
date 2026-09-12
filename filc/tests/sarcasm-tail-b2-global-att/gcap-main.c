#include <stdio.h>

extern long gcap_jump(long a, long b);
extern long gcap_owner(long a, long b);

/* Global bytes 4..11 are 0x3333333322222222; the tail adds a and b. */
static long expect(long a, long b) { return 0x3333333322222222L + a + b; }

int main(void)
{
    /* via the B2 join (misaligned global load inside the clone) */
    if (gcap_jump(100, 7) != expect(100, 7)) {
        printf("FAIL jump %lx\n", (unsigned long)gcap_jump(100, 7));
        return 1;
    }
    /* owner entry agrees */
    if (gcap_owner(100, 7) != expect(100, 7)) {
        printf("FAIL owner\n");
        return 1;
    }
    if (gcap_jump(-5, 3) != expect(-5, 3)) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 global att ok\n");
    return 0;
}
