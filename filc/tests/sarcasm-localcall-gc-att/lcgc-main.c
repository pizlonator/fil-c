#include <stdio.h>
#include <stdlib.h>
long lc_gc_sum(char** ptrs, long n);
#define N 8
#define OUTER 12000000
int main() {
    char** ptrs = malloc(N * sizeof(char*));
    long expect_one = 0;
    for (int j = 0; j < N; ++j) {
        char* s = malloc(4);
        s[0] = (char)(65 + j);
        s[1] = (char)(66 + j);
        s[2] = (char)(67 + j);
        s[3] = (char)(68 + j);
        ptrs[j] = s;
        expect_one += 65 + j + 66 + j + 67 + j + 68 + j;
    }
    long got = 0;
    for (int k = 0; k < OUTER; ++k)
        got += lc_gc_sum(ptrs, N);
    long expect = expect_one * OUTER;
    if (got == expect) printf("lcgc ok %ld\n", got);
    else printf("lcgc BAD %ld %ld\n", got, expect);
    return 0;
}
