#include <libm-alias-ldouble.h>
#include <pizlonated_math.h>

long double __scalbnl(long double x, int exp)
{
    return zmath_scalbnl(x, exp);
}

libm_alias_ldouble (__scalbn, scalbn)
