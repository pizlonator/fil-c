#include <stdio.h>
long andafter_test(long a, long b);
int main() {
    long r = andafter_test(40, 2);
    int ok = (r == 42);
    printf("andafter %s\n", ok ? "ok" : "BAD");
    return 0;
}
