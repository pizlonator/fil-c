#include <cpuid.h>
#include <stdio.h>
#include <stdbool.h>

#define TEST(reg, feature) do { \
        if (((reg) & bit_ ## feature)) \
            printf("%s: OK\n", #feature); \
        else { \
            printf("%s: missing\n", #feature); \
            all_good = false; \
        } \
    } while (false)

static void cpuid(unsigned* eax, unsigned* ebx, unsigned* ecx, unsigned* edx)
{
#ifdef __PIZLONATOR_WAS_HERE__
    unsigned level = *eax;
    __get_cpuid(level, eax, ebx, ecx, edx);
#else
    unsigned a = *eax, b, c = *ecx, d;
    asm volatile("cpuid\n\t" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    *eax = a;
    *ebx = b;
    *ecx = c;
    *edx = d;
#endif
}

int main()
{
    /* This really tests what simdutf calls "Icelake". But whatever. */
    
    unsigned eax = 0x7;
    unsigned ebx = 0;
    unsigned ecx = 0;
    unsigned edx = 0;

    cpuid(&eax, &ebx, &ecx, &edx);

    printf("eax = %x, ebx = %x, ecx = %x, edx = %x\n", eax, ebx, ecx, edx);

    bool all_good = true;

    TEST(ebx, AVX2);
    TEST(ebx, BMI);
    TEST(ebx, BMI2);
    TEST(ebx, AVX512BW);
    TEST(ebx, AVX512CD);
    TEST(ebx, AVX512VL);
    TEST(ecx, AVX512VBMI2);
    TEST(ecx, AVX512VPOPCNTDQ);

    return all_good ? 0 : 1;
}

