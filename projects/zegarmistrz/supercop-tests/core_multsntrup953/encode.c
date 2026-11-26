// 20250302 djb: port to -Wc++-compat
#include "crypto_encode.h"
#include "crypto_uint16.h"

void crypto_encode(unsigned char *s,const void *v)
{
  const crypto_uint16 *x = (const crypto_uint16 *) v;
  int i;

  for (i = 0;i < 953;++i) {
    crypto_uint16 u = *x++;
    *s++ = u;
    *s++ = u >> 8;
  }
}
