#ifndef Encode_H
#define Encode_H

#define Encode CRYPTO_NAMESPACE(Encode)

#include "crypto_uint16.h"

/* Encode(s,R,M,len) */
/* assumes 0 <= R[i] < M[i] < 16384 */
extern void Encode(unsigned char *,const crypto_uint16 *,const crypto_uint16 *,long long);

#endif
