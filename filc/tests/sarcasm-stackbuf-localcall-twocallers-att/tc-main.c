#include <stdio.h>
long tc_a(long a);
long tc_b(long a);
int main() {
    // Each caller's own buffer byte 0 is bumped by 0x01000000 in the clone:
    // tc_a: 0x11111111 + 0x01000000 = 0x12111111; tc_b likewise with 2s.
    if (tc_a(1) != 0x12111111) return 1;
    if (tc_b(1) != 0x23222222) return 1;
    printf("stackbuf localcall twocallers att ok\n");
    return 0;
}
