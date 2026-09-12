#include <sysdeps/x86/isa-level.h>
#if MINIMUM_X86_ISA_LEVEL >= AVX2_X86_ISA_LEVEL
# include <sysdeps/ieee754/dbl-64/w_exp.c>
#else
# include <sysdeps/../math/w_exp.c>
#endif
