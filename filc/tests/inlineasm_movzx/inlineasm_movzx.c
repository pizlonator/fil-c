#include <stdfil.h>
#include <stdint.h>

int main(void)
{
    /* MOVZX r16, r/m8 */
    uint8_t a8 = 0xab;
    uint16_t out16;
    asm volatile("movzbw %1, %0" : "=r"(out16) : "r"(a8));
    ZASSERT(out16 == 0xab);

    /* MOVZX r32, r/m8 */
    uint32_t out32;
    asm volatile("movzbl %1, %0" : "=r"(out32) : "r"(a8));
    ZASSERT(out32 == 0xab);

    /* MOVZX r64, r/m8 */
    uint64_t out64;
    asm volatile("movzbq %1, %0" : "=r"(out64) : "r"(a8));
    ZASSERT(out64 == 0xab);

    /* MOVZX r32, r/m16 */
    uint16_t a16 = 0xabcd;
    asm volatile("movzwl %1, %0" : "=r"(out32) : "r"(a16));
    ZASSERT(out32 == 0xabcd);

    /* MOVZX r64, r/m16 */
    asm volatile("movzwq %1, %0" : "=r"(out64) : "r"(a16));
    ZASSERT(out64 == 0xabcd);

    return 0;
}
