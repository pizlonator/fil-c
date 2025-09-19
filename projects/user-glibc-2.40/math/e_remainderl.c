#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_remainderl(long double x, long double y)
{
    return zmath_remainderl(x, y);
}

libm_alias_finite (__ieee754_remainderl, __remainderl)
