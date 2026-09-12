#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_logl (long double x)
{
    return zmath_logl (x);
}

long double __logl_finite (long double x)
{
    return zmath_logl_finite (x);
}

libm_alias_finite (__logl_finite, __logl)
