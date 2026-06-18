#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));

int main(void)
{
    v2df a = { -1.0, 2.0 };
    v2df b = { -3.0, -4.0 };

    int ra;
    int rb;

    asm volatile("movmskpd %1, %0" : "=r"(ra) : "x"(a));
    asm volatile("movmskpd %1, %0" : "=r"(rb) : "x"(b));

    ZASSERT(ra == 1);  /* only the first element is negative */
    ZASSERT(rb == 3);  /* both elements are negative */

    return 0;
}
