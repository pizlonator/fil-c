#include <stdio.h>

long fnA(unsigned long idx);

int main() {
    printf("so far so good\n");
    fflush(stdout);
    // One past the buffer's end for a 4-byte access (idx_max = 60): must trap.
    long v = fnA(61);
    printf("unexpectedly returned %ld\n", v);
    return 1;
}
