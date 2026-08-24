#include <stdio.h>

/* adcq $0 reads the carry cmpq produced (p < q unsigned); the instrumented
   incq (%rdi) between them must not clobber it. */
long incadc(long* p, long* q);

int main()
{
    long arr[2] = { 10, 20 };
    long c1 = incadc(&arr[0], &arr[1]);  /* p < q -> carry 1 */
    long c2 = incadc(&arr[1], &arr[0]);  /* p > q -> carry 0 */
    printf("%ld %ld %ld %ld\n", c1, c2, arr[0], arr[1]);  /* 1 0 11 21 */
    return 0;
}
