#include <stdio.h>

extern long tf_top(long a, long b);
extern long tf_top2(long a, long b);
extern long tf_top3(long a, long b);

int main(void)
{
    /* tf_top: je taken -> call tf_eq -> a*3 + b */
    if (tf_top(7, 7) != 28) {
        printf("FAIL top eq %ld\n", tf_top(7, 7));
        return 1;
    }
    /* tf_top: jne taken -> call tf_ne -> a*2 + b */
    if (tf_top(10, 3) != 23) {
        printf("FAIL top ne %ld\n", tf_top(10, 3));
        return 1;
    }
    /* tf_top2: B1 site, then local branch on the same flags */
    if (tf_top2(7, 7) != 28 || tf_top2(10, 3) != 23) {
        printf("FAIL top2 %ld %ld\n", tf_top2(7, 7), tf_top2(10, 3));
        return 1;
    }
    /* tf_top3: conditional B1 with a value-producing fallthrough */
    if (tf_top3(10, 3) != 23 || tf_top3(10, 10) != 15) {
        printf("FAIL top3 %ld %ld\n", tf_top3(10, 3), tf_top3(10, 10));
        return 1;
    }
    /* negatives */
    if (tf_top(-4, -4) != -16 || tf_top(-4, 9) != 1) {
        printf("FAIL neg %ld %ld\n", tf_top(-4, -4), tf_top(-4, 9));
        return 1;
    }
    printf("tailcall flags att ok\n");
    return 0;
}
