#include <libm-alias-float.h>

#define __llrintf __llrintf_ppc32
#undef libm_alias_float
#define libm_alias_float(a, b)
#include <sysdeps/powerpc/powerpc32/fpu/s_llrintf.c>
