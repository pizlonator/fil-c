#ifndef params_H
#define params_H

#define p 857
#define q 5167

#define q18 51 /* closest integer to 2^18/q */
#define q27 25976 /* closest integer to 2^27/q */

#include "crypto_decode_857xint16.h"
#define crypto_decode_pxint16 crypto_decode_857xint16

#include "crypto_encode_857xint16.h"
#define crypto_encode_pxint16 crypto_encode_857xint16

#endif
