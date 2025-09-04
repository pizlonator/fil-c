/* sm3.c - Functions to compute SM3 message digest of files or memory blocks
   according to the specification GM/T 004-2012 Cryptographic Hash Algorithm
   SM3, published by State Encryption Management Bureau, China.

   SM3 cryptographic hash algorithm.
   <http://www.sca.gov.cn/sca/xwdt/2010-12/17/content_1002389.shtml>

   Copyright (C) 2017-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Jia Zhang <qianyue.zj@alibaba-inc.com>, 2017,
   considerably copypasting from David Madore's sha256.c */

#include <config.h>

/* Specification.  */
#if HAVE_OPENSSL_SM3
# define GL_OPENSSL_INLINE _GL_EXTERN_INLINE
#endif
#include "sm3.h"

#include <stdint.h>
#include <string.h>

#include <byteswap.h>
#ifdef WORDS_BIGENDIAN
# define SWAP(n) (n)
#else
# define SWAP(n) bswap_32 (n)
#endif

#ifndef DEBUG_SM3
# define DEBUG_SM3 0
#endif

#if ! DEBUG_SM3
# define dbg_printf(fmt, ...) do { } while (0)
#else
# define dbg_printf printf
#endif

#if ! HAVE_OPENSSL_SM3

/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  */
static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */ };


/*
  Takes a pointer to a 256 bit block of data (eight 32 bit ints) and
  initializes it to the start constants of the SM3 algorithm.  This
  must be called before using hash in the call to sm3_hash
*/
void
sm3_init_ctx (struct sm3_ctx *ctx)
{
  ctx->state[0] = 0x7380166fUL;
  ctx->state[1] = 0x4914b2b9UL;
  ctx->state[2] = 0x172442d7UL;
  ctx->state[3] = 0xda8a0600UL;
  ctx->state[4] = 0xa96f30bcUL;
  ctx->state[5] = 0x163138aaUL;
  ctx->state[6] = 0xe38dee4dUL;
  ctx->state[7] = 0xb0fb0e4eUL;

  ctx->total[0] = ctx->total[1] = 0;
  ctx->buflen = 0;
}

/* Copy the value from v into the memory location pointed to by *cp,
   If your architecture allows unaligned access this is equivalent to
   * (uint32_t *) cp = v  */
static void
set_uint32 (char *cp, uint32_t v)
{
  memcpy (cp, &v, sizeof v);
}

/* Put result from CTX in first 32 bytes following RESBUF.  The result
   must be in little endian byte order.  */
void *
sm3_read_ctx (const struct sm3_ctx *ctx, void *resbuf)
{
  int i;
  char *r = resbuf;

  for (i = 0; i < 8; i++)
    set_uint32 (r + i * sizeof ctx->state[0], SWAP (ctx->state[i]));

  return resbuf;
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.  */
static void
sm3_conclude_ctx (struct sm3_ctx *ctx)
{
  /* Take yet unprocessed bytes into account.  */
  size_t bytes = ctx->buflen;
  size_t size = (bytes < 56) ? 64 / 4 : 64 * 2 / 4;

  /* Now count remaining bytes.  */
  ctx->total[0] += bytes;
  if (ctx->total[0] < bytes)
    ++ctx->total[1];

  /* Put the 64-bit file length in *bits* at the end of the buffer.
     Use set_uint32 rather than a simple assignment, to avoid risk of
     unaligned access.  */
  set_uint32 ((char *) &ctx->buffer[size - 2],
              SWAP ((ctx->total[1] << 3) | (ctx->total[0] >> 29)));
  set_uint32 ((char *) &ctx->buffer[size - 1],
              SWAP (ctx->total[0] << 3));

  memcpy (&((char *) ctx->buffer)[bytes], fillbuf, (size - 2) * 4 - bytes);

  /* Process last bytes.  */
  sm3_process_block (ctx->buffer, size * 4, ctx);
}

void *
sm3_finish_ctx (struct sm3_ctx *ctx, void *resbuf)
{
  sm3_conclude_ctx (ctx);
  return sm3_read_ctx (ctx, resbuf);
}

/* Compute SM3 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
void *
sm3_buffer (const char *buffer, size_t len, void *resblock)
{
  struct sm3_ctx ctx;

  /* Initialize the computation context.  */
  sm3_init_ctx (&ctx);

  /* Process whole buffer but last len % 64 bytes.  */
  sm3_process_bytes (buffer, len, &ctx);

  /* Put result in desired memory area.  */
  return sm3_finish_ctx (&ctx, resblock);
}

void
sm3_process_bytes (const void *buffer, size_t len, struct sm3_ctx *ctx)
{
  /* When we already have some bits in our internal buffer concatenate
     both inputs first.  */
  if (ctx->buflen != 0)
    {
      size_t left_over = ctx->buflen;
      size_t add = 128 - left_over > len ? len : 128 - left_over;

      memcpy (&((char *) ctx->buffer)[left_over], buffer, add);
      ctx->buflen += add;

      if (ctx->buflen > 64)
        {
          sm3_process_block (ctx->buffer, ctx->buflen & ~63, ctx);

          ctx->buflen &= 63;
          /* The regions in the following copy operation cannot overlap,
             because ctx->buflen < 64 ≤ (left_over + add) & ~63.  */
          memcpy (ctx->buffer,
                  &((char *) ctx->buffer)[(left_over + add) & ~63],
                  ctx->buflen);
        }

      buffer = (const char *) buffer + add;
      len -= add;
    }

  /* Process available complete blocks.  */
  if (len >= 64)
    {
#if !(_STRING_ARCH_unaligned || _STRING_INLINE_unaligned)
# define UNALIGNED_P(p) ((uintptr_t) (p) % alignof (uint32_t) != 0)
      if (UNALIGNED_P (buffer))
        while (len > 64)
          {
            sm3_process_block (memcpy (ctx->buffer, buffer, 64), 64, ctx);
            buffer = (const char *) buffer + 64;
            len -= 64;
          }
      else
#endif
        {
          sm3_process_block (buffer, len & ~63, ctx);
          buffer = (const char *) buffer + (len & ~63);
          len &= 63;
        }
    }

  /* Move remaining bytes in internal buffer.  */
  if (len > 0)
    {
      size_t left_over = ctx->buflen;

      memcpy (&((char *) ctx->buffer)[left_over], buffer, len);
      left_over += len;
      if (left_over >= 64)
        {
          sm3_process_block (ctx->buffer, 64, ctx);
          left_over -= 64;
          /* The regions in the following copy operation cannot overlap,
             because left_over ≤ 64.  */
          memcpy (ctx->buffer, &ctx->buffer[16], left_over);
        }
      ctx->buflen = left_over;
    }
}

/* --- Code below is the primary difference between sha256.c and sm3.c --- */

/* SM3 round constants */
#define T(j) sm3_round_constants[j]
static const uint32_t sm3_round_constants[64] = {
  0x79cc4519UL, 0xf3988a32UL, 0xe7311465UL, 0xce6228cbUL,
  0x9cc45197UL, 0x3988a32fUL, 0x7311465eUL, 0xe6228cbcUL,
  0xcc451979UL, 0x988a32f3UL, 0x311465e7UL, 0x6228cbceUL,
  0xc451979cUL, 0x88a32f39UL, 0x11465e73UL, 0x228cbce6UL,
  0x9d8a7a87UL, 0x3b14f50fUL, 0x7629ea1eUL, 0xec53d43cUL,
  0xd8a7a879UL, 0xb14f50f3UL, 0x629ea1e7UL, 0xc53d43ceUL,
  0x8a7a879dUL, 0x14f50f3bUL, 0x29ea1e76UL, 0x53d43cecUL,
  0xa7a879d8UL, 0x4f50f3b1UL, 0x9ea1e762UL, 0x3d43cec5UL,
  0x7a879d8aUL, 0xf50f3b14UL, 0xea1e7629UL, 0xd43cec53UL,
  0xa879d8a7UL, 0x50f3b14fUL, 0xa1e7629eUL, 0x43cec53dUL,
  0x879d8a7aUL, 0x0f3b14f5UL, 0x1e7629eaUL, 0x3cec53d4UL,
  0x79d8a7a8UL, 0xf3b14f50UL, 0xe7629ea1UL, 0xcec53d43UL,
  0x9d8a7a87UL, 0x3b14f50fUL, 0x7629ea1eUL, 0xec53d43cUL,
  0xd8a7a879UL, 0xb14f50f3UL, 0x629ea1e7UL, 0xc53d43ceUL,
  0x8a7a879dUL, 0x14f50f3bUL, 0x29ea1e76UL, 0x53d43cecUL,
  0xa7a879d8UL, 0x4f50f3b1UL, 0x9ea1e762UL, 0x3d43cec5UL,
};

/* Round functions.  */
#define FF1(X,Y,Z) ( X ^ Y ^ Z )
#define FF2(X,Y,Z) ( ( X & Y ) | ( X & Z ) | ( Y & Z ) )
#define GG1(X,Y,Z) ( X ^ Y ^ Z )
#define GG2(X,Y,Z) ( ( X & Y ) | ( ~X & Z ) )

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.
   Most of this code comes from David Madore's sha256.c.  */

void
sm3_process_block (const void *buffer, size_t len, struct sm3_ctx *ctx)
{
  const uint32_t *words = buffer;
  size_t nwords = len / sizeof (uint32_t);
  const uint32_t *endp = words + nwords;
  uint32_t x[16];
  uint32_t a = ctx->state[0];
  uint32_t b = ctx->state[1];
  uint32_t c = ctx->state[2];
  uint32_t d = ctx->state[3];
  uint32_t e = ctx->state[4];
  uint32_t f = ctx->state[5];
  uint32_t g = ctx->state[6];
  uint32_t h = ctx->state[7];
  uint32_t lolen = len;

  /* First increment the byte count.  GM/T 004-2012 specifies the possible
     length of the file up to 2^64 bits.  Here we only compute the
     number of bytes.  Do a double word increment.  */
  ctx->total[0] += lolen;
  ctx->total[1] += (len >> 31 >> 1) + (ctx->total[0] < lolen);

#define rol(x, n) (((x) << ((n) & 31)) | ((x) >> ((32 - (n)) & 31)))
#define P0(x) ((x)^rol(x,9)^rol(x,17))
#define P1(x) ((x)^rol(x,15)^rol(x,23))

#define W1(I) ( x[I&0x0f] )
#define W2(I) ( tw =   P1(x[I&0x0f]^x[(I-9)&0x0f]^rol(x[(I-3)&0x0f],15)) \
                     ^ rol(x[(I-13)&0x0f],7) ^ x[(I-6)&0x0f] \
                , x[I&0x0f] = tw )

#define R(i,A,B,C,D,E,F,G,H,T,W1,W2) \
  do { \
       if (++j) \
         dbg_printf("%2d %08x %08x %08x %08x %08x %08x %08x %08x\n", \
                    j-1, A, B, C, D, E, F, G, H); \
       ss1 = rol(rol(A,12) + E + T,7); \
       ss2 = ss1 ^ rol(A,12); \
       D += FF##i(A,B,C) + ss2 + (W1 ^ W2); \
       H += GG##i(E,F,G) + ss1 + W1; \
       B = rol(B,9); \
       F = rol(F,19); \
       H = P0(H); \
     } while(0)

#define R1(A,B,C,D,E,F,G,H,T,W1,W2) R(1,A,B,C,D,E,F,G,H,T,W1,W2)
#define R2(A,B,C,D,E,F,G,H,T,W1,W2) R(2,A,B,C,D,E,F,G,H,T,W1,W2)

  while (words < endp)
    {
      uint32_t tw;
      uint32_t ss1, ss2;
      int j;

      for (j = 0; j < 16; j++)
        {
          x[j] = SWAP (*words);
          words++;
        }

      j = -1;

      dbg_printf (" j    A        B        C        D        E  "
                  "      F        G        H\n"
                  "   %08x %08x %08x %08x %08x %08x %08x %08x\n",
                  a, b, c, d, e, f, g, h);

      R1( a, b, c, d, e, f, g, h, T( 0), W1( 0), W1( 4) );
      R1( d, a, b, c, h, e, f, g, T( 1), W1( 1), W1( 5) );
      R1( c, d, a, b, g, h, e, f, T( 2), W1( 2), W1( 6) );
      R1( b, c, d, a, f, g, h, e, T( 3), W1( 3), W1( 7) );
      R1( a, b, c, d, e, f, g, h, T( 4), W1( 4), W1( 8) );
      R1( d, a, b, c, h, e, f, g, T( 5), W1( 5), W1( 9) );
      R1( c, d, a, b, g, h, e, f, T( 6), W1( 6), W1(10) );
      R1( b, c, d, a, f, g, h, e, T( 7), W1( 7), W1(11) );
      R1( a, b, c, d, e, f, g, h, T( 8), W1( 8), W1(12) );
      R1( d, a, b, c, h, e, f, g, T( 9), W1( 9), W1(13) );
      R1( c, d, a, b, g, h, e, f, T(10), W1(10), W1(14) );
      R1( b, c, d, a, f, g, h, e, T(11), W1(11), W1(15) );
      R1( a, b, c, d, e, f, g, h, T(12), W1(12), W2(16) );
      R1( d, a, b, c, h, e, f, g, T(13), W1(13), W2(17) );
      R1( c, d, a, b, g, h, e, f, T(14), W1(14), W2(18) );
      R1( b, c, d, a, f, g, h, e, T(15), W1(15), W2(19) );
      R2( a, b, c, d, e, f, g, h, T(16), W1(16), W2(20) );
      R2( d, a, b, c, h, e, f, g, T(17), W1(17), W2(21) );
      R2( c, d, a, b, g, h, e, f, T(18), W1(18), W2(22) );
      R2( b, c, d, a, f, g, h, e, T(19), W1(19), W2(23) );
      R2( a, b, c, d, e, f, g, h, T(20), W1(20), W2(24) );
      R2( d, a, b, c, h, e, f, g, T(21), W1(21), W2(25) );
      R2( c, d, a, b, g, h, e, f, T(22), W1(22), W2(26) );
      R2( b, c, d, a, f, g, h, e, T(23), W1(23), W2(27) );
      R2( a, b, c, d, e, f, g, h, T(24), W1(24), W2(28) );
      R2( d, a, b, c, h, e, f, g, T(25), W1(25), W2(29) );
      R2( c, d, a, b, g, h, e, f, T(26), W1(26), W2(30) );
      R2( b, c, d, a, f, g, h, e, T(27), W1(27), W2(31) );
      R2( a, b, c, d, e, f, g, h, T(28), W1(28), W2(32) );
      R2( d, a, b, c, h, e, f, g, T(29), W1(29), W2(33) );
      R2( c, d, a, b, g, h, e, f, T(30), W1(30), W2(34) );
      R2( b, c, d, a, f, g, h, e, T(31), W1(31), W2(35) );
      R2( a, b, c, d, e, f, g, h, T(32), W1(32), W2(36) );
      R2( d, a, b, c, h, e, f, g, T(33), W1(33), W2(37) );
      R2( c, d, a, b, g, h, e, f, T(34), W1(34), W2(38) );
      R2( b, c, d, a, f, g, h, e, T(35), W1(35), W2(39) );
      R2( a, b, c, d, e, f, g, h, T(36), W1(36), W2(40) );
      R2( d, a, b, c, h, e, f, g, T(37), W1(37), W2(41) );
      R2( c, d, a, b, g, h, e, f, T(38), W1(38), W2(42) );
      R2( b, c, d, a, f, g, h, e, T(39), W1(39), W2(43) );
      R2( a, b, c, d, e, f, g, h, T(40), W1(40), W2(44) );
      R2( d, a, b, c, h, e, f, g, T(41), W1(41), W2(45) );
      R2( c, d, a, b, g, h, e, f, T(42), W1(42), W2(46) );
      R2( b, c, d, a, f, g, h, e, T(43), W1(43), W2(47) );
      R2( a, b, c, d, e, f, g, h, T(44), W1(44), W2(48) );
      R2( d, a, b, c, h, e, f, g, T(45), W1(45), W2(49) );
      R2( c, d, a, b, g, h, e, f, T(46), W1(46), W2(50) );
      R2( b, c, d, a, f, g, h, e, T(47), W1(47), W2(51) );
      R2( a, b, c, d, e, f, g, h, T(48), W1(48), W2(52) );
      R2( d, a, b, c, h, e, f, g, T(49), W1(49), W2(53) );
      R2( c, d, a, b, g, h, e, f, T(50), W1(50), W2(54) );
      R2( b, c, d, a, f, g, h, e, T(51), W1(51), W2(55) );
      R2( a, b, c, d, e, f, g, h, T(52), W1(52), W2(56) );
      R2( d, a, b, c, h, e, f, g, T(53), W1(53), W2(57) );
      R2( c, d, a, b, g, h, e, f, T(54), W1(54), W2(58) );
      R2( b, c, d, a, f, g, h, e, T(55), W1(55), W2(59) );
      R2( a, b, c, d, e, f, g, h, T(56), W1(56), W2(60) );
      R2( d, a, b, c, h, e, f, g, T(57), W1(57), W2(61) );
      R2( c, d, a, b, g, h, e, f, T(58), W1(58), W2(62) );
      R2( b, c, d, a, f, g, h, e, T(59), W1(59), W2(63) );
      R2( a, b, c, d, e, f, g, h, T(60), W1(60), W2(64) );
      R2( d, a, b, c, h, e, f, g, T(61), W1(61), W2(65) );
      R2( c, d, a, b, g, h, e, f, T(62), W1(62), W2(66) );
      R2( b, c, d, a, f, g, h, e, T(63), W1(63), W2(67) );

      dbg_printf("%2d %08x %08x %08x %08x %08x %08x %08x %08x\n",
                 j, a, b, c, d, e, f, g, h);

      a = ctx->state[0] ^= a;
      b = ctx->state[1] ^= b;
      c = ctx->state[2] ^= c;
      d = ctx->state[3] ^= d;
      e = ctx->state[4] ^= e;
      f = ctx->state[5] ^= f;
      g = ctx->state[6] ^= g;
      h = ctx->state[7] ^= h;
    }
}

#endif

/*
 * Hey Emacs!
 * Local Variables:
 * coding: utf-8
 * End:
 */
