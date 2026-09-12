#include <stdio.h>

extern long acmp_jump(long a, long b);
extern long acmp_owner(long a, long b);

int main(void)
{
    /* via the B2 join: banded slot (a) + jumper slot through the carrier
       (b) are different addresses, so the sum is a + b. A banding collapse
       would overwrite b with a and read 2a. */
    if (acmp_jump(100, 7) != 107) {
        printf("FAIL jump %ld\n", acmp_jump(100, 7));
        return 1;
    }
    /* control case: the owner tail addresses its own frame, so both reads
       coincide (slot holds a after the store) and the result is 2a. */
    if (acmp_owner(100, 7) != 200) {
        printf("FAIL owner %ld\n", acmp_owner(100, 7));
        return 1;
    }
    if (acmp_jump(-5, 3) != -2 || acmp_owner(-5, 3) != -10) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 addrcompare att ok\n");
    return 0;
}
