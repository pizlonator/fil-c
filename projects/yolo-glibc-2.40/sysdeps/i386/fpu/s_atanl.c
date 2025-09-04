/*
 * Public domain.
 *
 */

#include <libm-alias-ldouble.h>

long double
__atanl (long double x)
{
  long double res;

  asm ("fld1\n"
       "fpatan"
       : "=t" (res) : "0" (x));

  return res;
}

libm_alias_ldouble (__atan, atan)
