#include <stdio.h>

extern long nest_dispatch(long mode, long a, long b);
extern long nest_ssse3(long mode, long a, long b);
extern long nest_4x(long mode, long a, long b);
extern long nest_add_one(long mode, long a, long b);

int main(void)
{
    /* mode 0: base body a + b */
    if (nest_dispatch(0, 10, 3) != 13) {
        printf("FAIL base\n");
        return 1;
    }
    /* mode 1: ssse3 tail a*2 + b */
    if (nest_dispatch(1, 10, 3) != 23) {
        printf("FAIL ssse3\n");
        return 1;
    }
    /* mode 2: nested join ssse3 -> 4x tail a*4 + b */
    if (nest_dispatch(2, 10, 3) != 43) {
        printf("FAIL 4x\n");
        return 1;
    }
    /* mode 3: nested ENTRY jump (call+epilogue) a + 1 */
    if (nest_dispatch(3, 10, 3) != 11) {
        printf("FAIL entry\n");
        return 1;
    }
    /* owner entries agree */
    if (nest_ssse3(1, 10, 3) != 23 || nest_ssse3(2, 10, 3) != 43) {
        printf("FAIL ssse3 owner\n");
        return 1;
    }
    if (nest_4x(0, 10, 3) != 13 || nest_4x(2, 10, 3) != 43) {
        printf("FAIL 4x owner\n");
        return 1;
    }
    if (nest_add_one(9, 10, 3) != 11) {
        printf("FAIL add_one\n");
        return 1;
    }
    printf("tail b2 nested att ok\n");
    return 0;
}
