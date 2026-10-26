#include <stdfil.h>
#include <stdlib.h>

__attribute__((noinline))
static int uaf_in_loop(int *p, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) {
        p[i] = i;
        if (i == 4) {
            free(p);
        }
        s += p[i];
    }
    return s;
}

int main(void) {
    int *p = (int *)malloc(sizeof(int) * 16);
    int r = uaf_in_loop(p, 16);
    zprintf("Should not reach: %d\n", r);
    return 0;
}
