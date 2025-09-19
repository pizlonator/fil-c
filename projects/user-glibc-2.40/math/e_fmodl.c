#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_fmodl(long double x, long double y)
{
    return zmath_fmodl(x, y);
}

libm_alias_finite (__ieee754_fmodl, __fmodl)
