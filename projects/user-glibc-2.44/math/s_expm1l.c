#include <math.h>
#include <libm-alias-ldouble.h>
#include <pizlonated_math.h>

long double __expm1l(long double x)
{
    return zmath_expm1l(x);
}

libm_hidden_def (__expm1l)
libm_alias_ldouble (__expm1, expm1)
