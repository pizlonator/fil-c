#include <stdfil.h>

__attribute__((noinline))
static int diamond_loop(int *a, int n) {
    int acc = 0;
    for (int i = 0; i < n; ++i) {
        a[i] = i;
        if ((i & 1) == 0) {
            a[i] = 2;
        } else {
            a[i] = 4;
        }
        acc += a[i];
    }
    return acc;
}

int main(void) {
    int buf[128];
    int res = diamond_loop(buf, 128);
    zprintf("OK: diamond = %d\n", res);
    return (res == 384) ? 0 : 1;
}
