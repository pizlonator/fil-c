#include <math-svid-compat.h>
#include <math.h>
#include <libm-alias-float.h>

float
__lgammaf (float x)
{
  return __lgammaf_r (x, &__signgam);
}
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __lgammaf, lgammaf, GLIBC_2_43);
libm_alias_float_other (__lgamma, lgamma)
#else
libm_alias_float (__lgamma, lgamma)
strong_alias (__lgammaf, __gammaf)
weak_alias (__gammaf, gammaf)
#endif
