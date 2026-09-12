#include <libm-alias-ldouble.h>
#include <pizlonated_math.h>

long long __llrintl(long double x)
{
    return zmath_llrintl(x);
}

libm_alias_ldouble (__llrint, llrint)
strong_alias (__llrintl, __lrintl)
libm_alias_ldouble (__llrint, lrint)
