#include <stdfil.h>
#include <stdint.h>

int main(void)
{
    unsigned long a, b, lo, hi;
    unsigned __int128 prod;

    /* Two-operand form: dst *= src. */
    a = 6;
    b = 7;
    asm volatile("imul %1, %0"
                 : "+r"(a)
                 : "r"(b)
                 : "cc");
    ZASSERT(a == 42);

    /* Three-operand form: dst = src * imm. */
    asm volatile("imul $11, %1, %0"
                 : "=r"(a)
                 : "r"(b)
                 : "cc");
    ZASSERT(a == 77);

    /* One-operand form: RDX:RAX = RAX * src. */
    lo = 12345;
    b = 6789;
    asm volatile("imulq %2"
                 : "+a"(lo), "=d"(hi)
                 : "r"(b)
                 : "cc");
    prod = (unsigned __int128)12345 * (unsigned __int128)6789;
    ZASSERT(lo == (unsigned long)prod);
    ZASSERT(hi == (unsigned long)(prod >> 64));

    /* Bare mnemonic one-operand form: size inferred from the explicit 8-bit operand register. */
    uint8_t a8_bare = 12;
    register uint8_t b8_bare asm("cl") = 34;
    uint16_t result16_bare;
    asm volatile("imul %%cl"
                 : "=a"(result16_bare)
                 : "a"(a8_bare), "r"(b8_bare)
                 : "cc");
    ZASSERT(result16_bare == (uint16_t)a8_bare * (uint16_t)b8_bare);

    /* Bare mnemonic one-operand form: size inferred from the explicit 16-bit operand register. */
    uint16_t a16_bare = 1234;
    register uint16_t b16_bare asm("cx") = 5678;
    uint16_t lo16_bare, hi16_bare;
    asm volatile("imul %%cx"
                 : "=a"(lo16_bare), "=d"(hi16_bare)
                 : "a"(a16_bare), "r"(b16_bare)
                 : "cc");
    ZASSERT(lo16_bare == (uint16_t)((uint32_t)a16_bare * (uint32_t)b16_bare));
    ZASSERT(hi16_bare == (uint16_t)(((uint32_t)a16_bare * (uint32_t)b16_bare) >> 16));

    return 0;
}
