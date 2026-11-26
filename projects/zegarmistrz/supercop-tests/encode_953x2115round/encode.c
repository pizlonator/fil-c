// 20250302 djb: port to -Wc++-compat
#include "crypto_encode_953x2115.h"
#include "crypto_encode.h"
#include "crypto_int16.h"
#include "crypto_int8.h"
#define int16 crypto_int16

#define p 953

void crypto_encode(unsigned char *out,const void *v)
{
  const int16 *a = (const int16 *) v;
  int16 x[p];
  int i;

  for (i = 0;i < p;++i) x[i] = 3*((10923*a[i]+16384)>>15);
  crypto_encode_953x2115(out,x);
}
