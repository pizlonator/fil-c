#include <stdio.h>

extern long deep_jump(long a, long b);
extern long mid_frame(long a, long b);

/* (a + b) * 2 + 5 */
static long expect(long a, long b) { return (a + b) * 2 + 5; }

int main(void)
{
    /* via the epilogue-carrying B2 clone */
    if (deep_jump(10, 20) != expect(10, 20)) {
        printf("FAIL jump %ld\n", deep_jump(10, 20));
        return 1;
    }
    /* owner entry agrees */
    if (mid_frame(10, 20) != expect(10, 20)) {
        printf("FAIL owner\n");
        return 1;
    }
    if (deep_jump(-3, 7) != expect(-3, 7) || mid_frame(0, 0) != 5) {
        printf("FAIL more\n");
        return 1;
    }
    printf("tail b2 epilogue att ok\n");
    return 0;
}
