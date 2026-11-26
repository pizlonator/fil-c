// 20251220 djb: simplify usage of cryptoint
#include "crypto_core.h"
#include "params.h"

#include "crypto_int8.h"
#include "crypto_int16.h"
#include "crypto_int32.h"
#define int8 crypto_int8
#define int16 crypto_int16
#define int32 crypto_int32
typedef int8 small;

typedef int16 Fq;
/* always represented as -(q-1)/2...(q-1)/2 */

/* works for -7000000 < x < 7000000 if q in 4591, 4621, 5167, 6343, 7177, 7879 */
static Fq Fq_freeze(int32 x)
{
  x -= q*((q18*x)>>18);
  x -= q*((q27*x+67108864)>>27);
  return x;
}

int crypto_core(unsigned char *outbytes,const unsigned char *inbytes,const unsigned char *kbytes,const unsigned char *cbytes)
{
  Fq f[p];
  small g[p];
  Fq fg[p+p-1];
  int32 result;
  int i,j;

  crypto_decode_pxint16(f,inbytes);
  for (i = 0;i < p;++i)
    f[i] = Fq_freeze(f[i]);

  for (i = 0;i < p;++i) {
    small gi = kbytes[i];
    small gi0 = crypto_int8_bottombit_01(gi);
    g[i] = gi0-(gi&(gi0<<1));
  }

  for (i = 0;i < p;++i) {
    result = 0;
    for (j = 0;j <= i;++j) result += f[j]*(int32)g[i-j];
    fg[i] = Fq_freeze(result);
  }
  for (i = p;i < p+p-1;++i) {
    result = 0;
    for (j = i-p+1;j < p;++j) result += f[j]*(int32)g[i-j];
    fg[i] = Fq_freeze(result);
  }

  for (i = p+p-2;i >= p;--i) {
    fg[i-p] = Fq_freeze(fg[i-p]+fg[i]);
    fg[i-p+1] = Fq_freeze(fg[i-p+1]+fg[i]);
  }

  crypto_encode_pxint16(outbytes,fg);
  return 0;
}
