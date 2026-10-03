#include <stdio.h>

extern long call_a(long x);
extern long call_b(long x);
extern long mid_owner(long x);

int main(void)
{
    /* Each caller: r9 = 2*x + r11 + 3; result = r9 + r11.
       call_a(5):  r9 = 10+100+3 = 113,  result = 113+100  = 213.
       call_b(5):  r9 = 10+9000+3 = 9013, result = 9013+9000 = 18013.
       mid_owner(5): r9 = 10+7+3 = 20,    result = 20. */
    long ra = call_a(5);
    long rb = call_b(5);
    long ro = mid_owner(5);
    if (ra != 213 || rb != 18013 || ro != 20) {
        printf("FAIL: got %ld, %ld, %ld; want 213, 18013, 20\n", ra, rb, ro);
        return 1;
    }
    printf("localcall midbody twocallers att ok\n");
    return 0;
}