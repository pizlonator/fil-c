#ifndef params_H
#define params_H

#define p 1277
#define q 7879

#define q18 33 /* closest integer to 2^18/q */
#define q27 17035 /* closest integer to 2^27/q */

#include "crypto_decode_1277xint16.h"
#define crypto_decode_pxint16 crypto_decode_1277xint16

#include "crypto_encode_1277xint16.h"
#define crypto_encode_pxint16 crypto_encode_1277xint16

#endif
