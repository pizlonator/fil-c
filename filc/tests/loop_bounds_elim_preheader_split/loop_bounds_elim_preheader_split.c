#include <stdfil.h>

__attribute__((noinline))
static int multi_entry_loop(int *a, int n, int mode) {
    int i = 0;
    int s = 0;
    if (mode == 1) {
        a[0] = 5;
        goto loop_head;
    } else {
        a[0] = 5;
        goto loop_head;
    }
loop_head:
    for (; i < n; ++i) {
        a[i] = 5;
        s += a[i];
    }
    return s;
}

int main(void) {
    int buf[100];
    int res = multi_entry_loop(buf, 100, 1);
    zprintf("OK: preheader_split = %d\n", res);
    return (res == 500) ? 0 : 1;
}
