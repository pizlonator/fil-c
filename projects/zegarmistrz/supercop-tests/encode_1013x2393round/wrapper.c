#include "crypto_int16.h"
#include "crypto_encode.h"
#include "Encode.h"

#define q 7177
#define q12 3588
#define p 1013

void crypto_encode(unsigned char *s,const void *v)
{
  const crypto_int16 *r = v;
  crypto_uint16 R[p],M[p];
  int i;
  
  for (i = 0;i < p;++i) R[i] = (((r[i]+q12)&16383)*10923)>>15;
  for (i = 0;i < p;++i) M[i] = (q+2)/3;
  Encode(s,R,M,p);
}
