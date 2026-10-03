#include <stdio.h>

extern long cl_top(long a, long count, long step);
extern long cl_top2(long a, long count, long step);

int main(void)
{
    /* count=5, a=10, step=3: 5*(10+3) + 10 = 75 */
    if (cl_top(10, 5, 3) != 75) {
        printf("FAIL loop %ld\n", cl_top(10, 5, 3));
        return 1;
    }
    /* count=1: (10+3) + 10 = 23 */
    if (cl_top(10, 1, 3) != 23) {
        printf("FAIL one %ld\n", cl_top(10, 1, 3));
        return 1;
    }
    /* negative accumulation: 4*(-7 + -2) + -7 = -43 */
    if (cl_top(-7, 4, -2) != -43) {
        printf("FAIL neg %ld\n", cl_top(-7, 4, -2));
        return 1;
    }
    /* zero count: only the post-loop add runs */
    if (cl_top2(10, 5, 3) != 10) {
        printf("FAIL zero %ld\n", cl_top2(10, 5, 3));
        return 1;
    }
    printf("b2 clone loop att ok\n");
    return 0;
}
