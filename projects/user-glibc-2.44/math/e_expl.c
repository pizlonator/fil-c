#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_expl(long double x)
{
    return zmath_expl(x);
}

libm_alias_finite (__ieee754_expl, __expl)
