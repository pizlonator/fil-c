/*
 * Copyright (C) 2018 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "tls.h"

typedef uint8_t byte;
typedef uint32_t word32;
#define XMEMSET memset
#define XMEMCPY memcpy

/* from wolfssl-3.15.3/wolfcrypt/src/aes.c */

#ifdef UNUSED
static ALWAYS_INLINE void FlattenSzInBits(byte* buf, word32 sz)
{
    /* Multiply the sz by 8 */
//bbox: these sizes are never even close to 2^32/8
//    word32 szHi = (sz >> (8*sizeof(sz) - 3));
    sz <<= 3;

    /* copy over the words of the sz into the destination buffer */
//    buf[0] = (szHi >> 24) & 0xff;
//    buf[1] = (szHi >> 16) & 0xff;
//    buf[2] = (szHi >>  8) & 0xff;
//    buf[3] = szHi & 0xff;
    *(uint32_t*)(buf + 0) = 0;
//    buf[4] = (sz >> 24) & 0xff;
//    buf[5] = (sz >> 16) & 0xff;
//    buf[6] = (sz >>  8) & 0xff;
//    buf[7] = sz & 0xff;
    *(uint32_t*)(buf + 4) = SWAP_BE32(sz);
}
#endif

static void RIGHTSHIFTX(byte* x)
{
#define l ((unsigned long*)x)
#if 0

    // Generic byte-at-a-time algorithm
    int i;
    byte carryIn = (x[15] & 0x01) ? 0xE1 : 0;
    for (i = 0; i < AES_BLOCK_SIZE; i++) {
        byte carryOut = (x[i] << 7); // zero, or 0x80
        x[i] = (x[i] >> 1) ^ carryIn;
        carryIn = carryOut;
    }

#elif BB_BIG_ENDIAN

    // Big-endian can shift-right in larger than byte chunks
    // (we use the fact that 'x' is long-aligned)
    unsigned long carryIn = (x[15] & 0x01)
        ? ((unsigned long)0xE1 << (LONG_BIT-8))
        : 0;
# if ULONG_MAX <= 0xffffffff
    int i;
    for (i = 0; i < AES_BLOCK_SIZE/sizeof(long); i++) {
        unsigned long carryOut = l[i] << (LONG_BIT-1); // zero, or 0x800..00
        l[i] = (l[i] >> 1) ^ carryIn;
        carryIn = carryOut;
    }
# else
    // 64-bit code: need to process only 2 words
    unsigned long carryOut = l[0] << (LONG_BIT-1); // zero, or 0x800..00
    l[0] = (l[0] >> 1) ^ carryIn;
    l[1] = (l[1] >> 1) ^ carryOut;
# endif

#else /* LITTLE_ENDIAN */

    // In order to use word-sized ops, little-endian needs to byteswap.
    // On x86, code size increase is ~10 bytes compared to byte-by-byte.
    unsigned long carryIn = (x[15] & 0x01)
        ? ((unsigned long)0xE1 << (LONG_BIT-8))
        : 0;
# if ULONG_MAX <= 0xffffffff
    int i;
    for (i = 0; i < AES_BLOCK_SIZE/sizeof(long); i++) {
        unsigned long ti = SWAP_BE32(l[i]);
        unsigned long carryOut = ti << (LONG_BIT-1); // zero, or 0x800..00
        ti = (ti >> 1) ^ carryIn;
        l[i] = SWAP_BE32(ti);
        carryIn = carryOut;
    }
# else
    // 64-bit code: need to process only 2 words
    unsigned long tt = SWAP_BE64(l[0]);
    unsigned long carryOut = tt << (LONG_BIT-1); // zero, or 0x800..00
    tt = (tt >> 1) ^ carryIn; l[0] = SWAP_BE64(tt);
    tt = SWAP_BE64(l[1]);
    tt = (tt >> 1) ^ carryOut; l[1] = SWAP_BE64(tt);
# endif

#endif /* LITTLE_ENDIAN */
#undef l
}

// Caller guarantees X is aligned
static void GMULT(byte* X, byte* Y)
{
    byte Z[AES_BLOCK_SIZE] ALIGNED_long;
    //byte V[AES_BLOCK_SIZE] ALIGNED_long;
    int i;

    XMEMSET(Z, 0, AES_BLOCK_SIZE);
    //XMEMCPY(V, X, AES_BLOCK_SIZE);
    for (i = 0; i < AES_BLOCK_SIZE; i++) {
        uint32_t y = 0x800000 | Y[i];
        for (;;) { // for every bit in Y[i], from msb to lsb
            if (y & 0x80) {
                xorbuf_aligned_AES_BLOCK_SIZE(Z, X); // was V, not X
            }
            RIGHTSHIFTX(X); // was V, not X
            y = y << 1;
            if ((int32_t)y < 0) // if bit 0x80000000 set = if 8 iterations done
                break;
        }
    }
    XMEMCPY(X, Z, AES_BLOCK_SIZE);
}

//bbox:
// for TLS AES-GCM, a (which is AAD) is always 13 bytes long, and bbox code provides
// extra 3 zeroed bytes, making it a[16], or a[AES_BLOCK_SIZE].
// Resulting auth tag in s[] is also always AES_BLOCK_SIZE bytes.
//
// This allows some simplifications.
#define aSz 13
#define sSz AES_BLOCK_SIZE
void FAST_FUNC aesgcm_GHASH(byte* h,
    const byte* a, //unsigned aSz,
    const byte* c, unsigned cSz,
    byte* s //, unsigned sSz
)
{
    byte x[AES_BLOCK_SIZE] ALIGNED_long;
//    byte scratch[AES_BLOCK_SIZE] ALIGNED_long;
    unsigned blocks, partial;
    //was: byte* h = aes->H;

    //XMEMSET(x, 0, AES_BLOCK_SIZE);

    /* Hash in A, the Additional Authentication Data */
//    if (aSz != 0 && a != NULL) {
//        blocks = aSz / AES_BLOCK_SIZE;
//        partial = aSz % AES_BLOCK_SIZE;
//        while (blocks--) {
            //xorbuf(x, a, AES_BLOCK_SIZE);
            XMEMCPY(x, a, AES_BLOCK_SIZE);// memcpy(x,a) = memset(x,0)+xorbuf(x,a)
            GMULT(x, h);
//            a += AES_BLOCK_SIZE;
//        }
//        if (partial != 0) {
//            XMEMSET(scratch, 0, AES_BLOCK_SIZE);
//            XMEMCPY(scratch, a, partial);
//            xorbuf(x, scratch, AES_BLOCK_SIZE);
//            GMULT(x, h);
//        }
//    }

    /* Hash in C, the Ciphertext */
    if (cSz != 0 /*&& c != NULL*/) {
        blocks = cSz / AES_BLOCK_SIZE;
        partial = cSz % AES_BLOCK_SIZE;
        while (blocks--) {
            if (BB_UNALIGNED_MEMACCESS_OK) // c is not guaranteed to be aligned
                xorbuf_aligned_AES_BLOCK_SIZE(x, c);
            else
                xorbuf(x, c, AES_BLOCK_SIZE);
            GMULT(x, h);
            c += AES_BLOCK_SIZE;
        }
        if (partial != 0) {
            //XMEMSET(scratch, 0, AES_BLOCK_SIZE);
            //XMEMCPY(scratch, c, partial);
            //xorbuf(x, scratch, AES_BLOCK_SIZE);
            xorbuf(x, c, partial);//same result as above
            GMULT(x, h);
        }
    }

    /* Hash in the lengths of A and C in bits */
    //FlattenSzInBits(&scratch[0], aSz);
    //FlattenSzInBits(&scratch[8], cSz);
    //xorbuf_aligned_AES_BLOCK_SIZE(x, scratch);
    // simpler:
#define P32(v) ((uint32_t*)v)
  //P32(x)[0] ^= 0;
    P32(x)[1] ^= SWAP_BE32(aSz * 8);
  //P32(x)[2] ^= 0;
    P32(x)[3] ^= SWAP_BE32(cSz * 8);
#undef P32

    GMULT(x, h);

    /* Copy the result into s. */
    XMEMCPY(s, x, sSz);
}
