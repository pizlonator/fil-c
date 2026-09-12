#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_exp10l(long double x)
{
    return zmath_exp10l(x);
}

libm_alias_finite (__ieee754_exp10l, __exp10l)
