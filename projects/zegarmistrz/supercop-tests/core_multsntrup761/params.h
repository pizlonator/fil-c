#ifndef params_H
#define params_H

#define p 761
#define q 4591

#define q18 57 /* closest integer to 2^18/q */
#define q27 29235 /* closest integer to 2^27/q */

#include "crypto_decode_761xint16.h"
#define crypto_decode_pxint16 crypto_decode_761xint16

#include "crypto_encode_761xint16.h"
#define crypto_encode_pxint16 crypto_encode_761xint16

#endif
