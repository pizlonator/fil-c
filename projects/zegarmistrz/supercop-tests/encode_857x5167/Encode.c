#include "Encode.h"
#include "crypto_uint32.h"
#define uint16 crypto_uint16
#define uint32 crypto_uint32

/* 0 <= R[i] < M[i] < 16384 */
void Encode(unsigned char *out,const uint16 *R,const uint16 *M,long long len)
{
  if (len == 1) {
    uint16 r = R[0];
    uint16 m = M[0];
    while (m > 1) {
      *out++ = r;
      r >>= 8;
      m = (m+255)>>8;
    }
  }
  if (len > 1) {
    uint16 R2[(len+1)/2];
    uint16 M2[(len+1)/2];
    long long i;
    for (i = 0;i < len-1;i += 2) {
      uint32 m0 = M[i];
      uint32 r = R[i]+R[i+1]*m0;
      uint32 m = M[i+1]*m0;
      while (m >= 16384) {
        *out++ = r;
        r >>= 8;
        m = (m+255)>>8;
      }
      R2[i/2] = r;
      M2[i/2] = m;
    }
    if (i < len) {
      R2[i/2] = R[i];
      M2[i/2] = M[i];
    }
    Encode(out,R2,M2,(len+1)/2);
  }
}
