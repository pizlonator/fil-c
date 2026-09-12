#define NO_MATH_REDIRECT
#include <arm-ifunc.h>
#include <math.h>
#include <libm-alias-float.h>

extern __typeof (fmaf) __fmaf_vpfv4 attribute_hidden;
extern __typeof (fmaf) __fmaf_generic attribute_hidden;

arm_libm_ifunc (__fmaf, hwcap & HWCAP_ARM_VFPv4
			? __fmaf_vpfv4
			: __fmaf_generic)
libm_alias_float (__fma, fma)
