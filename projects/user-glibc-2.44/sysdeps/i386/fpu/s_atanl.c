/*
 * Public domain.
 *
 */

#include <libm-alias-ldouble.h>
#include <pizlonated_math.h>

long double
__atanl (long double x)
{
  return zmath_atanl (x);
}

libm_alias_ldouble (__atan, atan)
