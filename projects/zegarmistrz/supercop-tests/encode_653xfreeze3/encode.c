// 20250302 djb: port to -Wc++-compat
#include "crypto_encode.h"
#include "crypto_int16.h"
#define int16 crypto_int16

#define p 653

/* valid inputs: -16384 <= x < 16384 */
/* then 3 divides x-F3_freeze(x) */
/* and F3_freeze(x) is in {-1,0,1} */

/* all inputs: 3 divides x-F3_freeze(x) */
/* and F3_freeze(x) is in {-2,-1,0,1,2} */

static inline char F3_freeze(int16 x)
{
  return x-3*((10923*x+16384)>>15);
}

void crypto_encode(unsigned char *s,const void *v)
{
  const int16 *r = (const int16 *) v;

  int i;
  for (i = 0;i < p;++i)
    s[i] = F3_freeze(r[i]);
}
