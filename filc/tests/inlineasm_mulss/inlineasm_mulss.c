#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

int main(void)
{
    v4sf a = { 2.0f, 5.0f };
    v4sf b = { 3.0f, 7.0f };

    asm volatile("mulss %1, %0" : "+x"(a) : "x"(b));

    /* Only the low (first) element is multiplied; the high elements are unchanged. */
    ZASSERT(a[0] == 6.0f);
    ZASSERT(a[1] == 5.0f);

    return 0;
}
