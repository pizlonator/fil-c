#include <stdfil.h>

__attribute__((noinline))
static int sum_ptr_iv_oob(int *start, unsigned long n) {
    int s = 0;
    for (unsigned long i = 0; i < n; ++i) {
        int *p = start + i;
        s += *p;
    }
    return s;
}

int main(void) {
    int a[64];
    for (int i = 0; i < 64; ++i)
        a[i] = i;
    int res = sum_ptr_iv_oob(a, 65);
    zprintf("Should not reach: %d\n", res);
    return 0;
}
