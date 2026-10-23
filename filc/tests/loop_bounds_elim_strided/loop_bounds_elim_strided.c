#include <stdfil.h>

__attribute__((noinline))
static int strided_sum(int *arr, int count) {
    int s = 0;
    for (int k = 0; k < count; ++k) {
        int idx = k * 4;
        arr[idx] = 10;
        s += arr[idx];
    }
    return s;
}

int main(void) {
    int a[256];
    int res = strided_sum(a, 64);
    zprintf("OK: strided = %d\n", res);
    return (res == 640) ? 0 : 1;
}
