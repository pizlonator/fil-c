#include <libm-alias-ldouble.h>
#include <pizlonated_math.h>

long double __nearbyintl(long double x)
{
    return zmath_nearbyintl(x);
}

libm_alias_ldouble (__nearbyint, nearbyint)

