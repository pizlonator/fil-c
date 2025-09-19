#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_scalbl(long double x, long double exp)
{
    return zmath_scalbl(x, exp);
}

libm_alias_finite (__ieee754_scalbl, __scalbl)
