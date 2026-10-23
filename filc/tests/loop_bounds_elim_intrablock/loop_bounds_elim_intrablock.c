#include <stdfil.h>

__attribute__((noinline))
static int process_intrablock(int *p, int n) {
    int acc = 0;
    for (int i = 0; i < n; ++i) {
        int a = p[i];
        p[i] = a + 3;
        int b = p[i];
        p[i] = b * 2;
        acc += p[i];
    }
    return acc;
}

int main(void) {
    int buf[256];
    for (int i = 0; i < 256; ++i) buf[i] = 57;
    int res = process_intrablock(buf, 256);
    zprintf("OK: intrablock = %d\n", res);
    return (res == 30720) ? 0 : 1;
}
