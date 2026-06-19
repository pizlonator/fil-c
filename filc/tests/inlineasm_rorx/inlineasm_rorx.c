#include <stdfil.h>

static unsigned long ref_rorx(unsigned long src, unsigned count)
{
    unsigned y = count & 63;
    if (y == 0)
        return src;
    return (src >> y) | (src << (64 - y));
}

#define TEST(src, count) do { \
    unsigned long __src = (src); \
    unsigned long __dest; \
    asm volatile("rorx $" #count ", %1, %0" : "=r"(__dest) : "r"(__src)); \
    ZASSERT(__dest == ref_rorx(__src, (count))); \
} while (0)

int main(void)
{
    TEST(0x12345678UL, 4);
    TEST(0x8000000000000000UL, 1);
    TEST(0xdeadbeefcafebabeUL, 32);
    TEST(0xffffffffffffffffUL, 7);
    TEST(0x0000000000000001UL, 63);
    TEST(0x0UL, 5);
    TEST(0xff00ff00ff00ff00UL, 8);
    TEST(0xdeadbeefcafebabeUL, 0);
    TEST(0x00000000ffffffffUL, 16);
    TEST(0xa5a5a5a5a5a5a5a5UL, 4);
    return 0;
}
