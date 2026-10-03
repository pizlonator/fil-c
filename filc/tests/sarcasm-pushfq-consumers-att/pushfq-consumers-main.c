#include <stdio.h>

long po_setcc(long a);
long po_adc(long a, long b);
long po_cmov(long a);
long po_defined(long a);

static int fails = 0;

static void expect(const char *what, long got, long want) {
    if (got != want) {
        printf("%s BAD (got %ld want %ld)\n", what, got, want);
        fails++;
    } else {
        printf("%s ok\n", what);
    }
}

int main() {
    /* setcc on the restored ZF: a == -1 makes (a+1)==0 -> setne yields 0.
     * The je path adds 20, the fall-through 10. */
    expect("popfq feeds setcc ok", po_setcc(-1), 0 + 20);
    expect("popfq feeds setcc ne ok", po_setcc(41), 1 + 10);

    /* adc with the restored CF: b+3 first, then the restored CF adds one. */
    expect("popfq feeds adc ok", po_adc(3, 40), 40 + 3 + 1);
    expect("popfq feeds adc nc ok", po_adc(20, 40), 40 + 3);

    /* cmov on the restored ZF. */
    expect("popfq feeds cmov ok", po_cmov(0), 111 + 5);
    expect("popfq feeds cmov ne ok", po_cmov(7), 222 + 7 + 5);

    /* defined flags: js on the restored SF. */
    expect("popfq defined flags ok", po_defined(1), 1 + 9 + 500);
    expect("popfq defined flags ns ok", po_defined(0), 0 + 9);

    if (fails) return 1;
    printf("pushfq consumers all ok\n");
    return 0;
}
