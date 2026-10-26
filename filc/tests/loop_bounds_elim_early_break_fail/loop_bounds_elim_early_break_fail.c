#include <stdfil.h>

__attribute__((noinline))
static int sum_until_sentinel(int *arr, int max_scan, int sentinel) {
    int s = 0;
    for (int i = 0; i < max_scan; ++i) {
        int v = arr[i];
        if (v == sentinel)
            break;
        s += v;
    }
    return s;
}

int main(void) {
    int a[16];
    for (int i = 0; i < 16; ++i)
        a[i] = i + 1;
    /* Sentinel not present -> must trap at i=16 */
    int res = sum_until_sentinel(a, 1000, -999);
    zprintf("Should not reach: %d\n", res);
    return 0;
}
