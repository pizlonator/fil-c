#include <libm-alias-ldouble.h>
#include <pizlonated_math.h>

int __finitel(long double x)
{
    return zmath_finitel(x);
}

libm_alias_ldouble (__finite, finite)
