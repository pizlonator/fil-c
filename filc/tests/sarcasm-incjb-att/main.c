#include <stdio.h>

/* cmpq %rdi,%rsi leaves CF = (p < q) unsigned; incq (%rdi) must not disturb
   that pending carry across the inserted access check; jb consumes it. */
long incjb(long* p, long* q);

int main()
{
    long arr[2] = { 10, 20 };
    long taken = incjb(&arr[0], &arr[1]);     /* p < q: jb taken -> (10+1)+100 = 111 */
    long nottaken = incjb(&arr[1], &arr[0]);  /* p > q: not taken -> 20+1 = 21 */
    printf("%ld %ld %ld %ld\n", taken, nottaken, arr[0], arr[1]);  /* 111 21 11 21 */
    return 0;
}
