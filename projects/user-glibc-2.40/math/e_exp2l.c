#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_exp2l(long double x)
{
    return zmath_exp2l(x);
}

libm_alias_finite (__ieee754_exp2l, __exp2l)
