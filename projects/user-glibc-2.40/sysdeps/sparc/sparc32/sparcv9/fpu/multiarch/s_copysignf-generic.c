#include <libm-alias-float.h>
#define __copysignf __copysignf_generic
#undef libm_alias_float
#define libm_alias_float(a, b)
#include <sysdeps/ieee754/flt-32/s_copysignf.c>
