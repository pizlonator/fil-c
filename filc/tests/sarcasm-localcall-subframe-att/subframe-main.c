#include <stdio.h>
long subframe_main(long a, long b);
int main() {
    long r = subframe_main(10, 20);
    printf("subframe %ld %s\n", r, r == 648 ? "ok" : "BAD");
    return 0;
}
