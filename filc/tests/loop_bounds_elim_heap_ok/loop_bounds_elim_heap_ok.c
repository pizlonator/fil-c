#include <stdfil.h>
#include <stdlib.h>

__attribute__((noinline))
static int fill_and_sum(int *p, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) {
        p[i] = 8;
        s += p[i];
    }
    return s;
}

int main(void) {
    int *p = (int *)malloc(sizeof(int) * 256);
    int r = fill_and_sum(p, 256);
    free(p);
    zprintf("OK: heap_ok = %d\n", r);
    return (r == 2048) ? 0 : 1;
}
