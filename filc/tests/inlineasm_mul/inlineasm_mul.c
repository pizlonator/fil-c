#include <stdfil.h>
#include <stdint.h>

int main(void)
{
    /* mulb: AL * src, product in AX. */
    uint8_t a8 = 12;
    uint8_t b8 = 34;
    uint16_t result16;
    asm volatile("mulb %2"
                 : "=a"(result16)
                 : "a"(a8), "r"(b8)
                 : "cc");
    ZASSERT(result16 == (uint16_t)a8 * (uint16_t)b8);

    /* mulw: AX * src, product in DX:AX. */
    uint16_t a16 = 1234;
    uint16_t b16 = 5678;
    uint16_t lo16, hi16;
    asm volatile("mulw %3"
                 : "=a"(lo16), "=d"(hi16)
                 : "a"(a16), "r"(b16)
                 : "cc");
    ZASSERT(lo16 == (uint16_t)((uint32_t)a16 * (uint32_t)b16));
    ZASSERT(hi16 == (uint16_t)(((uint32_t)a16 * (uint32_t)b16) >> 16));

    /* mull: EAX * src, product in EDX:EAX. */
    uint32_t a32 = 123456;
    uint32_t b32 = 789012;
    uint32_t lo32, hi32;
    asm volatile("mull %3"
                 : "=a"(lo32), "=d"(hi32)
                 : "a"(a32), "r"(b32)
                 : "cc");
    ZASSERT(lo32 == (uint32_t)((uint64_t)a32 * (uint64_t)b32));
    ZASSERT(hi32 == (uint32_t)(((uint64_t)a32 * (uint64_t)b32) >> 32));

    /* mulq: RAX * src, product in RDX:RAX. */
    uint64_t a64 = 123456789;
    uint64_t b64 = 987654321;
    uint64_t lo64, hi64;
    asm volatile("mulq %3"
                 : "=a"(lo64), "=d"(hi64)
                 : "a"(a64), "r"(b64)
                 : "cc");
    ZASSERT(lo64 == (uint64_t)((unsigned __int128)a64 * (unsigned __int128)b64));
    ZASSERT(hi64 == (uint64_t)(((unsigned __int128)a64 * (unsigned __int128)b64) >> 64));

    /* Bare mnemonic: size inferred from the explicit 8-bit operand register. */
    uint8_t a8_bare = 12;
    register uint8_t b8_bare asm("cl") = 34;
    uint16_t result16_bare;
    asm volatile("mul %%cl"
                 : "=a"(result16_bare)
                 : "a"(a8_bare), "r"(b8_bare)
                 : "cc");
    ZASSERT(result16_bare == (uint16_t)a8_bare * (uint16_t)b8_bare);

    /* Bare mnemonic: size inferred from the explicit 16-bit operand register. */
    uint16_t a16_bare = 1234;
    register uint16_t b16_bare asm("cx") = 5678;
    uint16_t lo16_bare, hi16_bare;
    asm volatile("mul %%cx"
                 : "=a"(lo16_bare), "=d"(hi16_bare)
                 : "a"(a16_bare), "r"(b16_bare)
                 : "cc");
    ZASSERT(lo16_bare == (uint16_t)((uint32_t)a16_bare * (uint32_t)b16_bare));
    ZASSERT(hi16_bare == (uint16_t)(((uint32_t)a16_bare * (uint32_t)b16_bare) >> 16));

    return 0;
}
