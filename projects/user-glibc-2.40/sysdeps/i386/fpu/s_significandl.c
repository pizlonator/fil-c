/*
 * Public domain.
 */

#include <pizlonated_math.h>

long double
__significandl (long double x)
{
  return zmath_significandl (x);
}

weak_alias (__significandl, significandl)
