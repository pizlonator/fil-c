#include <stdfil.h>

__attribute__((noinline))
static int stride_oob(int *p, int steps) {
    int s = 0;
    for (int i = 0; i < steps; ++i) {
        int idx = i * 4;
        p[idx] = 1;
        s += p[idx];
    }
    return s;
}

int main(void) {
    int buf[20];
    int r = stride_oob(buf, 8);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
