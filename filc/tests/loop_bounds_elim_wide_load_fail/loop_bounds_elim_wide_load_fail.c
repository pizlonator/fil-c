#include <stdfil.h>

__attribute__((noinline))
static long wide_oob(char *bytes, int n) {
    long s = 0;
    for (int i = 0; i < n; ++i) {
        bytes[i] = 1;
        long *lp = (long *)(bytes + i);
        s += *lp;
    }
    return s;
}

int main(void) {
    char buf[16];
    long r = wide_oob(buf, 16);
    zprintf("Should not reach: %ld\n", r);
    return 0;
}
