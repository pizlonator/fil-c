#ifndef params_H
#define params_H

#define p 953
#define q 6343

#define q18 41 /* closest integer to 2^18/q */
#define q27 21160 /* closest integer to 2^27/q */

#include "crypto_decode_953xint16.h"
#define crypto_decode_pxint16 crypto_decode_953xint16

#include "crypto_encode_953xint16.h"
#define crypto_encode_pxint16 crypto_encode_953xint16

#endif
