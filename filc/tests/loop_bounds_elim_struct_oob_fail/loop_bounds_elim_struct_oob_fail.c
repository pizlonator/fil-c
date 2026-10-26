#include <stdfil.h>

struct Pair {
    long a;
    long b;
};

__attribute__((noinline))
static long sum_pairs_oob(struct Pair *p, int n) {
    long s = 0;
    for (int i = 0; i < n; ++i) {
        p[i].a = i;
        p[i].b = i * 2;
        s += p[i].a + p[i].b;
    }
    return s;
}

int main(void) {
    struct Pair arr[10];
    long r = sum_pairs_oob(arr, 11);
    zprintf("Should not reach: %ld\n", r);
    return 0;
}
