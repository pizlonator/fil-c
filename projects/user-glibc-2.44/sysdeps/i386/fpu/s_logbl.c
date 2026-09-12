/*
 * Public domain.
 */

#include <libm-alias-ldouble.h>
#include <pizlonated_math.h>

long double
__logbl (long double x)
{
  return zmath_logbl (x);
}

libm_alias_ldouble (__logb, logb)
