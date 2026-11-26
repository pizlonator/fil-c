#include "crypto_encode.h"

void crypto_encode(unsigned char *s,const void *v)
{
  const unsigned char *T = v;
  int i;
  for (i = 0;i < 128;++i)
    s[i] = T[2*i]+(T[2*i+1]<<4);
}
