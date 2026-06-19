#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));

int main(void)
{
    v2df a = { 2.0, 5.0 };
    v2df b = { 3.0, 7.0 };

    asm volatile("mulsd %1, %0" : "+x"(a) : "x"(b));

    /* Only the low (first) element is multiplied; the high element is unchanged. */
    ZASSERT(a[0] == 6.0);
    ZASSERT(a[1] == 5.0);

    return 0;
}
