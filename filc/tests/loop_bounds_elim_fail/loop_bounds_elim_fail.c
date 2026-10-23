#include <stdfil.h>

__attribute__((noinline))
static int sum_overflow(int *buf, int count)
{
    int s = 0;
    for (int i = 0; i < count; ++i) {
        s += buf[i];
    }
    return s;
}

int main(void)
{
    int small_buf[16];
    for (int i = 0; i < 16; ++i) {
        small_buf[i] = i;
    }

    /* Pass count > 16. Hoisted check will catch this and trigger filc safety error. */
    int total = sum_overflow(small_buf, 32);
    zprintf("Should not reach here: %d\n", total);
    return 0;
}
