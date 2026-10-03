#include <stdio.h>
#include <stdlib.h>

int oob_below(unsigned long idx);

int main() {
    // An index near 2^64: the single unsigned compare must trap (the offset
    // wraps to a negative buffer-relative value, never into the range).
    printf("got %d\n", oob_below(0xfffffffffffffff8ull));
    return 0;
}
