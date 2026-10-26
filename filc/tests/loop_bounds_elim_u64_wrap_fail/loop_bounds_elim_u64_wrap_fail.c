#include <stdfil.h>

__attribute__((noinline))
static int overflow_stride_u64(int *arr, unsigned long count) {
    int s = 0;
    for (unsigned long k = 0; k < count; ++k) {
        unsigned long idx = k * 0x1000000000000001ULL;
        s += arr[idx];
    }
    return s;
}

int main(void) {
    int a[64];
    for (int i = 0; i < 64; ++i)
        a[i] = i;
    /* At k=0, idx=0 (in bounds); at k=16 (BTC), 4*idx wraps 64-bit to 64 bytes (a[16], in bounds); k=1..15 OOB */
    int r = overflow_stride_u64(a, 17);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
