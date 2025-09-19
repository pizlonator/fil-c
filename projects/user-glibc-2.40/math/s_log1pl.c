#include <libm-alias-ldouble.h>
#include <pizlonated_math.h>

int __log1pl (long double x)
{
    return zmath_log1pl (x);
}

