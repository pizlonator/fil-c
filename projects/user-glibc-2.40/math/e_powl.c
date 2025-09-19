#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_powl(long double x, long double y)
{
    return zmath_powl(x, y);
}

libm_alias_finite (__ieee754_powl, __powl)
