// 20250302 djb: port to -Wc++-compat
#include "crypto_decode.h"
#include "crypto_uint16.h"

void crypto_decode(void *v,const unsigned char *s)
{
  crypto_uint16 *x = (crypto_uint16 *) v;
  int i;

  for (i = 0;i < 761;++i) {
    crypto_uint16 u0 = s[0];
    crypto_uint16 u1 = s[1];
    u1 <<= 8;
    *x = u0 | u1;
    x += 1;
    s += 2;
  }
}
