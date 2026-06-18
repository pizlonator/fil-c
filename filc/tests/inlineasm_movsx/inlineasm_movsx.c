#include <stdfil.h>
#include <stdint.h>

int main(void)
{
    int64_t out;

    /* MOVSX r64, r/m8 */
    int8_t a = -5;
    asm volatile("movsbq %1, %0" : "=r"(out) : "r"(a));
    ZASSERT(out == -5);

    /* MOVSX r64, r/m16 */
    int16_t b = -1234;
    asm volatile("movswq %1, %0" : "=r"(out) : "r"(b));
    ZASSERT(out == -1234);

    /* MOVSXD r64, r/m32 */
    int32_t c = -12345678;
    asm volatile("movslq %1, %0" : "=r"(out) : "r"(c));
    ZASSERT(out == -12345678);

    return 0;
}
