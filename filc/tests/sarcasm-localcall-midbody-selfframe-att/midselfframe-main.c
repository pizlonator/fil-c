#include <stdio.h>

extern long midself(long a, long b);

int main(void)
{
    /* -8(%rbp) = 2, clone adds 100 -> 102; -16(%rbp) = 3, clone triples ->
       9; the 0(%rsp) slot = 11, clone rewrites to 77.
       result = 102 + 9 + 77 = 188. */
    long r = midself(2, 3);
    if (r != 188) {
        printf("FAIL: got %ld, want 188\n", r);
        return 1;
    }
    printf("localcall midbody selfframe att ok\n");
    return 0;
}