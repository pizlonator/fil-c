#include <stdfil.h>
#include <arm_acle.h>

int main(void)
{
    unsigned count = 0;
    unsigned i;
    for (i = 0; i < 100; ++i) {
        __builtin_arm_yield();
        count++;
    }
    ZASSERT(count == 100);
    __builtin_arm_yield();
    __builtin_arm_yield();
    __yield();
    zprintf("yield ok\n");
    return 0;
}
