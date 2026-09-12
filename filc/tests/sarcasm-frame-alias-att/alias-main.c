#include <stdio.h>
long alias_mov(long a, long b);
long alias_lea(long a, long b);
int main() {
    // alias_mov: 0+0 through the mov-parked rsp alias: 10+20+123.
    // alias_lea: through the lea-parked rsp-32 alias: 100+200.
    long m = alias_mov(10, 20);
    long l = alias_lea(100, 200);
    printf("alias %ld %ld %s\n", m, l, (m == 153 && l == 300) ? "ok" : "BAD");
    return 0;
}
