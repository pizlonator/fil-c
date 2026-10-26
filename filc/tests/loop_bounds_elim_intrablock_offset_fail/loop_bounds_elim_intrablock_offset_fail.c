#include <stdfil.h>

__attribute__((noinline))
static int intrablock_neighbor_oob(int *p, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) {
        int a = p[i];
        int b = p[i + 1];
        s += a + b;
    }
    return s;
}

int main(void) {
    int buf[16];
    for (int i = 0; i < 16; ++i) buf[i] = i;
    int r = intrablock_neighbor_oob(buf, 16);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
