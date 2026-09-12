#include <stdio.h>
long andafter2_test(long a, long b);
int main() {
    long r = andafter2_test(40, 2);
    int ok = (r == 42);
    printf("andafter2 %s\n", ok ? "ok" : "BAD");
    return 0;
}
