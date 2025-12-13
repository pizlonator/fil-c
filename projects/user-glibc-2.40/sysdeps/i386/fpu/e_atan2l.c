/*
 * Public domain.
 *
 */

#include <math_private.h>
#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double
__ieee754_atan2l (long double y, long double x)
{
  return zmath_atan2l (y, x);
}
libm_alias_finite (__ieee754_atan2l, __atan2l)
