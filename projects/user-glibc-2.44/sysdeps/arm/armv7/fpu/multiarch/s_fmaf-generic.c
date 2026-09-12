#include <libm-alias-float.h>
#define __fmaf __fmaf_generic
#undef libm_alias_float
#define libm_alias_float(a, b)
#include <sysdeps/ieee754/dbl-64/s_fmaf.c>
