#ifdef __x86_64__

#include <cpuid.h>
#include <stdio.h>
#include <stdbool.h>

/* Probe for AVX512-BF16 (VCVTNE2PS2BF16/VCVTNEPS2BF16/VPDPBF16 & co).
   Unlike most AVX512 bits, BF16 lives in CPUID leaf 7 SUBLEAF 1, and in EAX
   (bit 5) rather than EBX/ECX/EDX; that's also where cpuid.h's
   bit_AVX512BF16 is documented to apply ("Features in %eax for leaf 7
   sub-leaf 1"). Mirrors has_avx512.c. */

static void do_cpuid(unsigned* eax, unsigned* ebx, unsigned* ecx, unsigned* edx)
{
#ifdef __PIZLONATOR_WAS_HERE__
    unsigned level = *eax;
    unsigned subleaf = *ecx;
    __get_cpuid_count(level, subleaf, eax, ebx, ecx, edx);
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
    unsigned eax = 0x7;
    unsigned ebx = 0;
    unsigned ecx = 0x1; /* subleaf 1 */
    unsigned edx = 0;

    do_cpuid(&eax, &ebx, &ecx, &edx);

    printf("eax = %x, ebx = %x, ecx = %x, edx = %x\n", eax, ebx, ecx, edx);

    bool all_good = true;
    if (eax & bit_AVX512BF16)
        printf("AVX512BF16: OK\n");
    else {
        printf("AVX512BF16: missing\n");
        all_good = false;
    }

    return all_good ? 0 : 1;
}

#else

int main()
{
    return 1;
}

#endif
