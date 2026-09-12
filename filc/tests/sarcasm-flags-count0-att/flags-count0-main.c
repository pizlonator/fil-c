#include <stdio.h>
#include <stdlib.h>
long flagcount0(long* b, long n);
int main() {
    long* buf = malloc(8);
    buf[0] = 7;
    // n=3 is not greater than 5 (small path, 0); n=10 is (big path,
    // buf[0]+100 = 107). The $0 shift/rotate between the checked load and
    // the branch must preserve the cmp's flags: without the count-0 model
    // the injected check's flag residue feeds the jg and the n=10 case
    // prints 0 instead of 107.
    printf("%ld %ld\n", flagcount0(buf, 3), flagcount0(buf, 10));
    return 0;
}
