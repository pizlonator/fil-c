#include <stdfil.h>

__attribute__((noinline))
static int overflow_stride_i32(int *arr, int count) {
    int s = 0;
    for (int k = 0; k < count; ++k) {
        int idx = (int)((unsigned int)k * 0x40000001u);
        s += arr[idx];
    }
    return s;
}

int main(void) {
    int a[16];
    for (int i = 0; i < 16; ++i)
        a[i] = i;
    /* At k=0, idx=0 (in bounds); at k=4 (BTC), idx=4 (in bounds); at k=1, idx=0x40000001 (OOB) */
    int r = overflow_stride_i32(a, 5);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
