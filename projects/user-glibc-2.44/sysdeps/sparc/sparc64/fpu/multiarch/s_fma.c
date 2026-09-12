#define NO_MATH_REDIRECT
#include <sparc-ifunc.h>
#define dfmal __hide_dfmal
#define f32xfmaf64 __hide_f32xfmaf64
#include <math.h>
#undef dfmal
#undef f32xfmaf64
#include <libm-alias-double.h>
#include <math-narrow-alias.h>

extern __typeof (fma) __fma_vis3 attribute_hidden;
extern __typeof (fma) __fma_generic attribute_hidden;

sparc_libm_ifunc (__fma,
		  hwcap & HWCAP_SPARC_FMAF
		  ? __fma_vis3
		  : __fma_generic);
libm_alias_double (__fma, fma)
libm_alias_double_narrow (__fma, fma)
