/*
Implementation by the Keccak Team, namely, Guido Bertoni, Joan Daemen,
Michaël Peeters, Gilles Van Assche and Ronny Van Keer, 
hereby denoted as "the implementer".

For more information, feedback or questions, please refer to our website:
https://keccak.team/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include <string.h>
#include "crypto_core.h"
#include "KeccakP-1600-SnP.h"

#ifdef KeccakP1600_useLaneComplementing
typedef unsigned long long UINT64;
static void KeccakP1600_ApplyLaneComplementing(const unsigned char *state)
{
    ((UINT64*)state)[ 1] ^= ~(UINT64)0;
    ((UINT64*)state)[ 2] ^= ~(UINT64)0;
    ((UINT64*)state)[ 8] ^= ~(UINT64)0;
    ((UINT64*)state)[12] ^= ~(UINT64)0;
    ((UINT64*)state)[17] ^= ~(UINT64)0;
    ((UINT64*)state)[20] ^= ~(UINT64)0;
}
#endif

int crypto_core(unsigned char *out, const unsigned char *in, const unsigned char *k, const unsigned char *c)
{
    if (out != in)
        memcpy(out, in, 200);
#ifdef KeccakP1600_useLaneComplementing
    KeccakP1600_ApplyLaneComplementing(out);
#endif
    KeccakP1600_Permute_24rounds(out);
#ifdef KeccakP1600_useLaneComplementing
    KeccakP1600_ApplyLaneComplementing(out);
#endif
    return 0;
}
