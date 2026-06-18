#include <stdfil.h>
#include <stdint.h>

int main(void)
{
    /* divb: AX / divisor, quotient in AL, remainder in AH. */
    uint16_t dividend8 = 100;
    uint8_t divisor8 = 7;
    uint16_t result8;
    asm volatile("divb %2"
                 : "=a"(result8)
                 : "a"(dividend8), "r"(divisor8)
                 : "cc");
    ZASSERT((result8 & 0xff) == 14);
    ZASSERT((result8 >> 8) == 2);

    /* divw: DX:AX / divisor, quotient in AX, remainder in DX. */
    uint16_t dividend16 = 100;
    uint16_t divisor16 = 7;
    uint16_t quotient16, remainder16;
    asm volatile("divw %4"
                 : "=a"(quotient16), "=d"(remainder16)
                 : "a"(dividend16), "d"(0), "r"(divisor16)
                 : "cc");
    ZASSERT(quotient16 == 14);
    ZASSERT(remainder16 == 2);

    /* divl: EDX:EAX / divisor, quotient in EAX, remainder in EDX. */
    uint32_t dividend32 = 100;
    uint32_t divisor32 = 7;
    uint32_t quotient32, remainder32;
    asm volatile("divl %4"
                 : "=a"(quotient32), "=d"(remainder32)
                 : "a"(dividend32), "d"(0U), "r"(divisor32)
                 : "cc");
    ZASSERT(quotient32 == 14);
    ZASSERT(remainder32 == 2);

    /* divq: RDX:RAX / divisor, quotient in RAX, remainder in RDX. */
    uint64_t dividend64 = 100;
    uint64_t divisor64 = 7;
    uint64_t quotient64, remainder64;
    asm volatile("divq %4"
                 : "=a"(quotient64), "=d"(remainder64)
                 : "a"(dividend64), "d"(0ULL), "r"(divisor64)
                 : "cc");
    ZASSERT(quotient64 == 14);
    ZASSERT(remainder64 == 2);

    /* Bare mnemonic: size inferred from the explicit 8-bit operand register. */
    uint16_t dividend8_bare = 100;
    register uint8_t divisor8_bare asm("cl") = 7;
    uint16_t result8_bare;
    asm volatile("div %%cl"
                 : "=a"(result8_bare)
                 : "a"(dividend8_bare), "r"(divisor8_bare)
                 : "cc");
    ZASSERT((result8_bare & 0xff) == 14);
    ZASSERT((result8_bare >> 8) == 2);

    /* Bare mnemonic: size inferred from the explicit 16-bit operand register. */
    uint16_t dividend16_bare = 100;
    register uint16_t divisor16_bare asm("cx") = 7;
    uint16_t quotient16_bare, remainder16_bare;
    asm volatile("div %%cx"
                 : "=a"(quotient16_bare), "=d"(remainder16_bare)
                 : "a"(dividend16_bare), "d"(0), "r"(divisor16_bare)
                 : "cc");
    ZASSERT(quotient16_bare == 14);
    ZASSERT(remainder16_bare == 2);

    return 0;
}
