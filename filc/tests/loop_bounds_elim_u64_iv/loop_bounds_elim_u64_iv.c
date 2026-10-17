#include <stdfil.h>
#include <stddef.h>

__attribute__((noinline))
static size_t sum_u64(size_t *a, size_t n) {
    size_t s = 0;
    for (size_t i = 0; i < n; ++i) {
        a[i] = 2;
        s += a[i];
    }
    return s;
}

int main(void) {
    size_t buf[512];
    size_t r = sum_u64(buf, 512);
    zprintf("OK: u64_iv = %zu\n", r);
    return (r == 1024) ? 0 : 1;
}
