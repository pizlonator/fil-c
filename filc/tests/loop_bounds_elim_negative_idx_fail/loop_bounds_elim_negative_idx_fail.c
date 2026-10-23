#include <stdfil.h>

__attribute__((noinline))
static int countdown_underflow(int *p, int n) {
    int s = 0;
    for (int i = n - 1; i >= -1; --i) {
        p[i] = 1;
        s += p[i];
    }
    return s;
}

int main(void) {
    int buf[16];
    int r = countdown_underflow(buf, 16);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
