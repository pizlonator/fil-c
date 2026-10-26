#include <stdfil.h>

__attribute__((noinline))
static int rev_sum(int *arr, int n) {
    int s = 0;
    for (int i = n - 1; i >= 0; --i) {
        arr[i] = i;
        s += arr[i];
    }
    return s;
}

int main(void) {
    int a[128];
    int res = rev_sum(a, 128);
    zprintf("OK: reverse = %d\n", res);
    return (res == 8128) ? 0 : 1;
}
