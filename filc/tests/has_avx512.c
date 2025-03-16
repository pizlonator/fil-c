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

int main()
{
    /* This really tests what simdutf calls "Icelake". But whatever. */
    
    unsigned eax;
    unsigned ebx;
    unsigned ecx;
    unsigned edx;

    int result = __get_cpuid(7, &eax, &ebx, &ecx, &edx);

    printf("result = %d, eax = %x, ebx = %x, ecx = %x, edx = %x\n", result, eax, ebx, ecx, edx);

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

