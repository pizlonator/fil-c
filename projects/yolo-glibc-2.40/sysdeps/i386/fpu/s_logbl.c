/*
 * Public domain.
 */

#include <libm-alias-ldouble.h>

long double
__logbl (long double x)
{
  long double res;

  asm ("fxtract\n"
       "fstp	%%st" : "=t" (res) : "0" (x));
  return res;
}

libm_alias_ldouble (__logb, logb)
