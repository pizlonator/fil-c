#include <stdio.h>

long repz_retq_fn(long x);
long rep_retq_fn(long x);
long repe_retq_fn(long x);

int main(void)
{
    /* Functions ending in a spelled `repz retq` / `rep retq` / `repe retq`
       must assemble and return exactly like `rep ret` (issue #370). */
    long a = repz_retq_fn(5);
    long b = rep_retq_fn(7);
    long c = repe_retq_fn(9);
    if (a != 22 || b != 36 || c != 110) {
        printf("bad %ld %ld %ld\n", a, b, c);
        return 1;
    }
    printf("rep retq att ok\n");
    return 0;
}