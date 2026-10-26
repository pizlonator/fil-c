#include <stdfil.h>

__attribute__((noinline))
static int run_zero_or_one(int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) {
        a[i] = 42;
        s += a[i];
    }
    return s;
}

int main(void) {
    int buf[1] = {0};
    int z = run_zero_or_one(buf, 0);
    int o = run_zero_or_one(buf, 1);
    if (z != 0 || o != 42) return 1;
    zprintf("OK: zero_trip = %d\n", o);
    return 0;
}
