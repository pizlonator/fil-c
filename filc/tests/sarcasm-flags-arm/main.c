#include <stdio.h>
long h(long a, long b, long *p);
int main() {
    long x = 100;
    printf("%ld %ld\n", h(3, 3, &x), h(3, 4, &x));  /* expect 105 100 */
    return 0;
}
