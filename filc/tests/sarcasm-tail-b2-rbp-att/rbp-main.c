#include <stdio.h>

extern long rbp_jump(long a, long b);
extern long rbp_owner(long a, long b);

/* a + b + 7 */
int main(void)
{
    if (rbp_jump(10, 20) != 37) {
        printf("FAIL jump %ld\n", rbp_jump(10, 20));
        return 1;
    }
    if (rbp_owner(10, 20) != 37 || rbp_jump(-5, 5) != 7) {
        printf("FAIL owner\n");
        return 1;
    }
    printf("tail b2 rbp att ok\n");
    return 0;
}
