// 20250302 djb: port to -Wc++-compat
#include "crypto_encode.h"
#include "crypto_uint8.h"
#define uint8 crypto_uint8

#define p 653

void crypto_encode(unsigned char *s,const void *v)
{
  const uint8 *f = (const uint8 *) v;
  uint8 x;
  int i;

  for (i = 0;i < p/4;++i) {
    x = *f++ + 1;
    x += (*f++ + 1)<<2;
    x += (*f++ + 1)<<4;
    x += (*f++ + 1)<<6;
    *s++ = x;
  }
  x = *f++ + 1;
  *s++ = x;
}
