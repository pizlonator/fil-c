#define NO_MATH_REDIRECT
#include <arm-ifunc.h>
#define dfmal __hide_dfmal
#define f32xfmaf64 __hide_f32xfmaf64
#include <math.h>
#undef dfmal
#undef f32xfmaf64
#undef fma
#include <libm-alias-double.h>
#include <math-narrow-alias.h>

extern __typeof (fma) __fma_vpfv4 attribute_hidden;
extern __typeof (fma) __fma_generic attribute_hidden;

arm_libm_ifunc (__fma, hwcap & HWCAP_ARM_VFPv4
		       ? __fma_vpfv4
		       : __fma_generic);
libm_alias_double (__fma, fma)
libm_alias_double_narrow (__fma, fma)
