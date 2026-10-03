#include <stdio.h>

extern long outer_midfn(long x);

int main(void)
{
    /* r10 = 5; the .Lmidsub clone computes r9 = 2*5+3 = 13; result = r9. */
    long r = outer_midfn(5);
    if (r != 13) {
        printf("FAIL: got %ld, want 13\n", r);
        return 1;
    }
    printf("localcall midfn att ok\n");
    return 0;
}