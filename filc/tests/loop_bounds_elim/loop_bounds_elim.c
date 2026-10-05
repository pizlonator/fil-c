#include <stdfil.h>

static int array[1024];

__attribute__((noinline))
static int sum_array(int *buf, int count)
{
    int s = 0;
    for (int i = 0; i < count; ++i) {
        s += buf[i];
    }
    return s;
}

int main(void)
{
    for (int i = 0; i < 1024; ++i) {
        array[i] = i + 1;
    }

    int total = sum_array(array, 1024);
    if (total != (1024 * 1025) / 2) {
        zprintf("FAILURE: expected %d, got %d\n", (1024 * 1025) / 2, total);
        return 1;
    }

    zprintf("OK: loop bounds elim sum = %d\n", total);
    return 0;
}
