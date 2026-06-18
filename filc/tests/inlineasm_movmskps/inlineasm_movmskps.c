#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

int main(void)
{
    v4sf a = { -1.0f, 2.0f, -3.0f, 4.0f };
    v4sf b = { -1.0f, -2.0f, -3.0f, -4.0f };
    v4sf c = { 1.0f, 2.0f, 3.0f, 4.0f };

    int ra;
    int rb;
    int rc;

    asm volatile("movmskps %1, %0" : "=r"(ra) : "x"(a));
    asm volatile("movmskps %1, %0" : "=r"(rb) : "x"(b));
    asm volatile("movmskps %1, %0" : "=r"(rc) : "x"(c));

    ZASSERT(ra == 5);   /* elements 0 and 2 are negative */
    ZASSERT(rb == 15);  /* all elements are negative */
    ZASSERT(rc == 0);   /* no elements are negative */

    return 0;
}
