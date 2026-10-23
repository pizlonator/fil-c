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
    a[5] = -999;
    /* max_scan (1000) > 16, but loop exits at i=5 so it never touches OOB */
    int res = sum_until_sentinel(a, 1000, -999);
    zprintf("OK: early break sum = %d\n", res);
    return (res == 15) ? 0 : 1;
}
