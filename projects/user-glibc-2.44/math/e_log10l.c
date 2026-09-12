#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_log10l (long double x)
{
    return zmath_log10l (x);
}

long double __log10l_finite (long double x)
{
    return zmath_log10l_finite (x);
}

libm_alias_finite (__log10l_finite, __log10l)
