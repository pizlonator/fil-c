#include <stdfil.h>
#include <stdlib.h>

__attribute__((noinline))
static int touch_zero_alloc(int *p, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) {
        p[i] = 1;
        s += p[i];
    }
    return s;
}

int main(void) {
    int *p = (int *)malloc(0);
    int r = touch_zero_alloc(p, 1);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
