#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_log2l (long double x)
{
    return zmath_log2l (x);
}

long double __log2l_finite (long double x)
{
    return zmath_log2l_finite (x);
}

libm_alias_finite (__log2l_finite, __log2l)
