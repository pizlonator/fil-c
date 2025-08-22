#include <cpuid.h>
#include <inttypes.h>
#include <stdfil.h>

static inline void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
                         uint32_t *edx) {
  uint32_t level = *eax;
  __get_cpuid(level, eax, ebx, ecx, edx);
}

int main()
{
    uint32_t eax = 1;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);
    zprintf("%u, %u, %u, %u\n", eax, ebx, ecx, edx);
    return 0;
}

