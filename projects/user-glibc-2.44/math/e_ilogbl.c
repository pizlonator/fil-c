#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double __ieee754_ilogbl(long double x)
{
    return zmath_ilogbl(x);
}

