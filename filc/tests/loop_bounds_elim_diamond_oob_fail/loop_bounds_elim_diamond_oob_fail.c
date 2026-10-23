#include <stdfil.h>

__attribute__((noinline))
static int diamond_oob(int *a, int n) {
    int acc = 0;
    for (int i = 0; i < n; ++i) {
        a[i] = i;
        if (i == n - 1) {
            acc += a[i + 1];
        } else {
            acc += a[i];
        }
    }
    return acc;
}

int main(void) {
    int buf[32];
    int r = diamond_oob(buf, 32);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
