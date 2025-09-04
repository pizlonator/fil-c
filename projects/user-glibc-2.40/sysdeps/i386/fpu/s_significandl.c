/*
 * Public domain.
 */


long double
__significandl (long double x)
{
  long double res;

  asm ("fxtract\n"
       "fstp	%%st(1)" : "=t" (res) : "0" (x));
  return res;
}

weak_alias (__significandl, significandl)
