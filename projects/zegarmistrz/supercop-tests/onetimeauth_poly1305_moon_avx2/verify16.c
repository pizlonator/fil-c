#include "crypto_verify.h"
#include "crypto_int16.h"

int crypto_verify(const unsigned char *x,const unsigned char *y)
{
  crypto_int16 differentbits = 0;
  int i;

  for (i = 0;i < crypto_verify_BYTES;++i)
    differentbits |= x[i] ^ y[i];

  return crypto_int16_nonzero_mask(differentbits);
}
