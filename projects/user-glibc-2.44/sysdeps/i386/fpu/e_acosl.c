/*
 * Public domain.
 *
 */

#include <math_private.h>
#include <libm-alias-finite.h>
#include <pizlonated_math.h>

long double
__ieee754_acosl (long double x)
{
  return zmath_acosl (x);
}
libm_alias_finite (__ieee754_acosl, __acosl)
