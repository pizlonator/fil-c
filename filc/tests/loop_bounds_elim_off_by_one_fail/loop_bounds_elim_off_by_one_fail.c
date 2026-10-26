#include <stdfil.h>

__attribute__((noinline))
static int off_by_one(int *p, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) {
        p[i] = i;
        s += p[i];
    }
    return s;
}

int main(void) {
    int buf[32];
    int r = off_by_one(buf, 33);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
