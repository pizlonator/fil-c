#include <stdio.h>

extern void caller6(long *out);

int main(void)
{
    /* The .Lsum5_mid clone runs in caller6 with caller6's registers:
       rax = 0, rdx..r9 = 2..5, so the store writes 0+2+3+4+5 = 14. */
    long out = 0;
    caller6(&out);
    if (out != 14) {
        printf("FAIL: got %ld, want 14\n", out);
        return 1;
    }
    printf("localcall into func mid ok\n");
    return 0;
}