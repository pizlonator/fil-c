/* kyber.c - the Kyber key encapsulation mechanism (main part)
 * Copyright (C) 2024 g10 Code GmbH
 *
 * This file was modified for use by Libgcrypt.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * You can also use this file under the same licence of original code.
 * SPDX-License-Identifier: CC0 OR Apache-2.0
 *
 */
/*
  Original code from:

  Repository: https://github.com/pq-crystals/kyber.git
  Branch: standard
  Commit: 11d00ff1f20cfca1f72d819e5a45165c1e0a2816

  Licence:
  Public Domain (https://creativecommons.org/share-your-work/public-domain/cc0/);
  or Apache 2.0 License (https://www.apache.org/licenses/LICENSE-2.0.html).

  Authors:
        Joppe Bos
        Léo Ducas
        Eike Kiltz
        Tancrède Lepoint
        Vadim Lyubashevsky
        John Schanck
        Peter Schwabe
        Gregor Seiler
        Damien Stehlé

  Kyber Home: https://www.pq-crystals.org/kyber/
 */
/*
 * This implementation consists of four files: kyber.h (header),
 * kyber.c (this), kyber-common.c (common part), and kyber-kdep.c
 * (KYBER_K dependent part).
 *
 * It is for inclusion in libgcrypt library.  Also, standalone use of
 * the implementation is possible.  With KYBER_K defined, it can offer
 * the variant of that KYBER_K specified.  Otherwise, three variants
 * are offered.
 *
 * From original code, following modification was made.
 *
 * - C++ style comments are changed to C-style.
 *
 * - No use of KYBER_NAMESPACE and FIPS202_NAMESPACE.  Don't export
 *   internal symbols.
 *
 * - "verify" routine is changed to return 1 on success, and now has
 *   new name "verify1", so that the use of the routine won't need
 *   negation (since negation might result non-constant-time code with
 *   branch by some compiler).
 *
 * - For "xof" routines, definitions of xof_init and xof_close are
 *   added, so that memory will be possible to be cleared after its
 *   use.
 *
 * - Different external API for shake128, having _init and _close.
 *
 * - New implementation of kyber_shake128_absorb, with the shake128
 *   API.
 *
 * - Added an external function: shake256v with variable arguments.
 *
 * - Macro definitions of xof_squeezeblocks, prf, and rkprf are
 *   modified to use the shake128 API and the shake256v function.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef _GCRYPT_IN_LIBGCRYPT
#include <stdarg.h>
#include <gpg-error.h>

#include "types.h"
#include "g10lib.h"
#include "gcrypt-int.h"
#include "const-time.h"
#include "kyber.h"

static int crypto_kem_keypair_2(uint8_t *pk, uint8_t *sk);
static int crypto_kem_keypair_3(uint8_t *pk, uint8_t *sk);
static int crypto_kem_keypair_4(uint8_t *pk, uint8_t *sk);

static int crypto_kem_enc_2(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
static int crypto_kem_enc_3(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
static int crypto_kem_enc_4(uint8_t *ct, uint8_t *ss, const uint8_t *pk);

static int crypto_kem_dec_2(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);
static int crypto_kem_dec_3(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);
static int crypto_kem_dec_4(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

void
kyber_keypair (int algo, uint8_t *pk, uint8_t *sk)
{
  switch (algo)
    {
    case GCRY_KEM_MLKEM512:
      crypto_kem_keypair_2 (pk, sk);
      break;
    case GCRY_KEM_MLKEM768:
    default:
      crypto_kem_keypair_3 (pk, sk);
      break;
    case GCRY_KEM_MLKEM1024:
      crypto_kem_keypair_4 (pk, sk);
      break;
    }
}

void
kyber_encap (int algo, uint8_t *ct, uint8_t *ss, const uint8_t *pk)
{
  switch (algo)
    {
    case GCRY_KEM_MLKEM512:
      crypto_kem_enc_2 (ct, ss, pk);
      break;
    case GCRY_KEM_MLKEM768:
    default:
      crypto_kem_enc_3 (ct, ss, pk);
      break;
    case GCRY_KEM_MLKEM1024:
      crypto_kem_enc_4 (ct, ss, pk);
      break;
    }
}

void
kyber_decap (int algo, uint8_t *ss, const uint8_t *ct, const uint8_t *sk)
{
  switch (algo)
    {
    case GCRY_KEM_MLKEM512:
      crypto_kem_dec_2 (ss, ct, sk);
      break;
    case GCRY_KEM_MLKEM768:
    default:
      crypto_kem_dec_3 (ss, ct, sk);
      break;
    case GCRY_KEM_MLKEM1024:
      crypto_kem_dec_4 (ss, ct, sk);
      break;
    }
}

static void
randombytes (uint8_t *out, size_t outlen)
{
  _gcry_randomize (out, outlen, GCRY_VERY_STRONG_RANDOM);
}

typedef struct {
  gcry_md_hd_t h;
} keccak_state;

static void
shake128_init (keccak_state *state)
{
  gcry_err_code_t ec;

  ec = _gcry_md_open (&state->h, GCRY_MD_SHAKE128, 0);
  if (ec)
    log_fatal ("internal md_open failed: %d\n", ec);
}

static void
shake128_absorb (keccak_state *state, const uint8_t *in, size_t inlen)
{
  _gcry_md_write (state->h, in, inlen);
}

static void
shake128_finalize (keccak_state *state)
{
  (void)state;
}

static void
shake128_squeeze (keccak_state *state, uint8_t *out, size_t outlen)
{
  _gcry_md_extract (state->h, GCRY_MD_SHAKE128, out, outlen);
}

static void
shake128_close (keccak_state *state)
{
  _gcry_md_close (state->h);
}

#define MAX_ARGS 16
static void
shake256v (uint8_t *out, size_t outlen, ...)
{
  gcry_buffer_t iov[MAX_ARGS];
  va_list ap;
  int i;
  void *p;
  size_t len;

  va_start (ap, outlen);
  for (i = 0; i < MAX_ARGS; i++)
    {
      p = va_arg (ap, void *);
      len = va_arg (ap, size_t);
      if (!p)
        break;

      iov[i].size = 0;
      iov[i].data = p;
      iov[i].off = 0;
      iov[i].len = len;
    }
  va_end (ap);

  _gcry_md_hash_buffers_extract (GCRY_MD_SHAKE256, 0, out, outlen,
                                 iov, i);
}

static void
sha3_256 (uint8_t h[32], const uint8_t *in, size_t inlen)
{
  _gcry_md_hash_buffer (GCRY_MD_SHA3_256, h, in, inlen);
}

static void
sha3_512 (uint8_t h[64], const uint8_t *in, size_t inlen)
{
  _gcry_md_hash_buffer (GCRY_MD_SHA3_512, h, in, inlen);
}

#define verify1 ct_memequal
#define cmov    ct_memmov_cond
#else
#include "kyber.h"

void randombytes (uint8_t *out, size_t outlen);

typedef struct {
  uint64_t s[25];
  unsigned int pos;
} keccak_state;

void shake128_init (keccak_state *state);
void shake128_absorb (keccak_state *state, const uint8_t *in, size_t inlen);
void shake128_finalize (keccak_state *state);
void shake128_squeeze (keccak_state *state, uint8_t *out, size_t outlen);
void shake128_close (keccak_state *state);

void shake256v (uint8_t *out, size_t outlen, ...);
void sha3_256 (uint8_t h[32], const uint8_t *in, size_t inlen);
void sha3_512 (uint8_t h[64], const uint8_t *in, size_t inlen);

/* Return 1 when success, 0 otherwise.  */
unsigned int verify1 (const uint8_t *a, const uint8_t *b, size_t len);
/* Conditional move.  */
void cmov (uint8_t *r, const uint8_t *x, size_t len, uint8_t b);
#endif

/*************** kyber/ref/fips202.h */
#define SHAKE128_RATE 168

/*************** kyber/ref/params.h */
#define KYBER_N 256
#define KYBER_Q 3329

#define KYBER_SYMBYTES 32   /* size in bytes of hashes, and seeds */
#define KYBER_SSBYTES  32   /* size in bytes of shared key */

#define KYBER_POLYBYTES          384

#define KYBER_ETA2 2

#define KYBER_INDCPA_MSGBYTES       (KYBER_SYMBYTES)

/* KYBER_K dependent values (part 1) */
#define KYBER_ETA1_2   3
#define KYBER_ETA1_3_4 2

#define KYBER_POLYCOMPRESSEDBYTES_2_3 128
#define KYBER_POLYCOMPRESSEDBYTES_4   160

/*************** kyber/ref/poly.h */
/*
 * Elements of R_q = Z_q[X]/(X^n + 1). Represents polynomial
 * coeffs[0] + X*coeffs[1] + X^2*coeffs[2] + ... + X^{n-1}*coeffs[n-1]
 */
typedef struct{
  int16_t coeffs[KYBER_N];
} poly;

#if !defined(KYBER_K) || KYBER_K == 2 || KYBER_K == 3
static void poly_compress_128(uint8_t r[KYBER_POLYCOMPRESSEDBYTES_2_3], const poly *a);
static void poly_decompress_128(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES_2_3]);
#endif
#if !defined(KYBER_K) || KYBER_K == 4
static void poly_compress_160(uint8_t r[KYBER_POLYCOMPRESSEDBYTES_4], const poly *a);
static void poly_decompress_160(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES_4]);
#endif
static void poly_tobytes(uint8_t r[KYBER_POLYBYTES], const poly *a);
static void poly_frombytes(poly *r, const uint8_t a[KYBER_POLYBYTES]);

static void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES]);
static void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *r);
#if !defined(KYBER_K) || KYBER_K == 2
static void poly_getnoise_eta1_2(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce);
#endif
#if !defined(KYBER_K) || KYBER_K == 3 || KYBER_K == 4
static void poly_getnoise_eta1_3_4(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce);
#endif
static void poly_getnoise_eta2(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce);

static void poly_ntt(poly *r);
static void poly_invntt_tomont(poly *r);
static void poly_basemul_montgomery(poly *r, const poly *a, const poly *b);
static void poly_tomont(poly *r);

static void poly_reduce(poly *r);

static void poly_add(poly *r, const poly *a, const poly *b);
static void poly_sub(poly *r, const poly *a, const poly *b);

/*************** kyber/ref/ntt.h */
static const int16_t zetas[128];

static void ntt(int16_t poly[256]);

static void invntt(int16_t poly[256]);

static void basemul(int16_t r[2], const int16_t a[2], const int16_t b[2], int16_t zeta);

/*************** kyber/ref/reduce.h */
#define MONT -1044 /* 2^16 mod q */
#define QINV -3327 /* q^-1 mod 2^16 */

static int16_t montgomery_reduce(int32_t a);

static int16_t barrett_reduce(int16_t a);

/*************** kyber/ref/symmetric.h */
typedef keccak_state xof_state;

static void kyber_shake128_absorb (keccak_state *state,
                                   const uint8_t seed[KYBER_SYMBYTES],
                                   uint8_t x, uint8_t y)
{
  shake128_absorb (state, seed, KYBER_SYMBYTES);
  shake128_absorb (state, &x, 1);
  shake128_absorb (state, &y, 1);
  shake128_finalize (state);
}

#define XOF_BLOCKBYTES SHAKE128_RATE

#define hash_h(OUT, IN, INBYTES) sha3_256(OUT, IN, INBYTES)
#define hash_g(OUT, IN, INBYTES) sha3_512(OUT, IN, INBYTES)
#define xof_init(STATE) shake128_init(STATE)
#define xof_close(STATE) shake128_close(STATE)
#define xof_absorb(STATE, SEED, X, Y) kyber_shake128_absorb(STATE, SEED, X, Y)
#define xof_squeezeblocks(OUT, OUTBLOCKS, STATE) shake128_squeeze(STATE, OUT, SHAKE128_RATE * OUTBLOCKS)
#define prf(OUT, OUTBYTES, KEY, NONCE) \
  shake256v(OUT, OUTBYTES, (void *)(KEY), (size_t)KYBER_SYMBYTES, \
			   (void *)&(NONCE), (size_t)1, \
			   NULL, (size_t)0)
#define rkprf(OUT, KEY, INPUT) \
  shake256v(OUT, KYBER_SSBYTES, (void *)(KEY), (size_t)KYBER_SYMBYTES, \
				(void *)(INPUT), (size_t)KYBER_CIPHERTEXTBYTES, \
				NULL, (size_t)0)

#include "kyber-common.c"

#define VARIANT2(name) name ## _2
#define VARIANT3(name) name ## _3
#define VARIANT4(name) name ## _4

/* KYBER_K dependent values (part 2) */
#define KYBER_POLYVECBYTES      (KYBER_K * KYBER_POLYBYTES)
#define KYBER_INDCPA_PUBLICKEYBYTES (KYBER_POLYVECBYTES + KYBER_SYMBYTES)
#define KYBER_INDCPA_SECRETKEYBYTES (KYBER_POLYVECBYTES)
#define KYBER_INDCPA_BYTES          (KYBER_POLYVECCOMPRESSEDBYTES + KYBER_POLYCOMPRESSEDBYTES)

#define KYBER_PUBLICKEYBYTES  (KYBER_INDCPA_PUBLICKEYBYTES)
/* 32 bytes of additional space to save H(pk) */
#define KYBER_SECRETKEYBYTES  (KYBER_INDCPA_SECRETKEYBYTES + KYBER_INDCPA_PUBLICKEYBYTES + 2*KYBER_SYMBYTES)
#define KYBER_CIPHERTEXTBYTES (KYBER_INDCPA_BYTES)

#ifdef KYBER_K
# if KYBER_K == 2
#  define KYBER_POLYCOMPRESSEDBYTES    128
#  define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 320)
#  define poly_compress poly_compress_128
#  define poly_decompress poly_decompress_128
#  define poly_getnoise_eta1 poly_getnoise_eta1_2
# elif KYBER_K == 3
#  define KYBER_POLYCOMPRESSEDBYTES    128
#  define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 320)
#  define poly_compress poly_compress_128
#  define poly_decompress poly_decompress_128
#  define poly_getnoise_eta1 poly_getnoise_eta1_3_4
# elif KYBER_K == 4
#  define KYBER_POLYCOMPRESSEDBYTES    160
#  define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 352)
#  define poly_compress poly_compress_160
#  define poly_decompress poly_decompress_160
#  define poly_getnoise_eta1 poly_getnoise_eta1_3_4
# endif
# include "kyber-kdep.c"
# else
# define KYBER_K 2
# define KYBER_POLYCOMPRESSEDBYTES    128
# define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 320)
# define poly_compress poly_compress_128
# define poly_decompress poly_decompress_128
# define poly_getnoise_eta1 poly_getnoise_eta1_2
# define crypto_kem_keypair_derand VARIANT2(crypto_kem_keypair_derand)
# define crypto_kem_enc_derand VARIANT2(crypto_kem_enc_derand)
# define crypto_kem_keypair VARIANT2(crypto_kem_keypair)
# define crypto_kem_enc VARIANT2(crypto_kem_enc)
# define crypto_kem_dec VARIANT2(crypto_kem_dec)
# define polyvec VARIANT2(polyvec)
# define polyvec_compress VARIANT2(polyvec_compress)
# define polyvec_decompress VARIANT2(polyvec_decompress)
# define polyvec_tobytes VARIANT2(polyvec_tobytes)
# define polyvec_frombytes VARIANT2(polyvec_frombytes)
# define polyvec_ntt VARIANT2(polyvec_ntt)
# define polyvec_invntt_tomont VARIANT2(polyvec_invntt_tomont)
# define polyvec_basemul_acc_montgomery VARIANT2(polyvec_basemul_acc_montgomery)
# define polyvec_reduce VARIANT2(polyvec_reduce)
# define polyvec_add VARIANT2(polyvec_add)
# define pack_pk VARIANT2(pack_pk)
# define unpack_pk VARIANT2(unpack_pk)
# define pack_sk VARIANT2(pack_sk)
# define unpack_sk VARIANT2(unpack_sk)
# define pack_ciphertext VARIANT2(pack_ciphertext)
# define unpack_ciphertext VARIANT2(unpack_ciphertext)
# define gen_matrix VARIANT2(gen_matrix)
# define indcpa_keypair_derand VARIANT2(indcpa_keypair_derand)
# define indcpa_enc VARIANT2(indcpa_enc)
# define indcpa_dec VARIANT2(indcpa_dec)
# include "kyber-kdep.c"

# define KYBER_K 3
# define KYBER_POLYCOMPRESSEDBYTES    128
# define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 320)
# define poly_compress poly_compress_128
# define poly_decompress poly_decompress_128
# define poly_getnoise_eta1 poly_getnoise_eta1_3_4
# define crypto_kem_keypair_derand VARIANT3(crypto_kem_keypair_derand)
# define crypto_kem_enc_derand VARIANT3(crypto_kem_enc_derand)
# define crypto_kem_keypair VARIANT3(crypto_kem_keypair)
# define crypto_kem_enc VARIANT3(crypto_kem_enc)
# define crypto_kem_dec VARIANT3(crypto_kem_dec)
# define polyvec VARIANT3(polyvec)
# define polyvec_compress VARIANT3(polyvec_compress)
# define polyvec_decompress VARIANT3(polyvec_decompress)
# define polyvec_tobytes VARIANT3(polyvec_tobytes)
# define polyvec_frombytes VARIANT3(polyvec_frombytes)
# define polyvec_ntt VARIANT3(polyvec_ntt)
# define polyvec_invntt_tomont VARIANT3(polyvec_invntt_tomont)
# define polyvec_basemul_acc_montgomery VARIANT3(polyvec_basemul_acc_montgomery)
# define polyvec_reduce VARIANT3(polyvec_reduce)
# define polyvec_add VARIANT3(polyvec_add)
# define pack_pk VARIANT3(pack_pk)
# define unpack_pk VARIANT3(unpack_pk)
# define pack_sk VARIANT3(pack_sk)
# define unpack_sk VARIANT3(unpack_sk)
# define pack_ciphertext VARIANT3(pack_ciphertext)
# define unpack_ciphertext VARIANT3(unpack_ciphertext)
# define gen_matrix VARIANT3(gen_matrix)
# define indcpa_keypair_derand VARIANT3(indcpa_keypair_derand)
# define indcpa_enc VARIANT3(indcpa_enc)
# define indcpa_dec VARIANT3(indcpa_dec)
# include "kyber-kdep.c"

# define KYBER_K 4
# define KYBER_POLYCOMPRESSEDBYTES    160
# define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * 352)
# define poly_compress poly_compress_160
# define poly_decompress poly_decompress_160
# define poly_getnoise_eta1 poly_getnoise_eta1_3_4
# define crypto_kem_keypair_derand VARIANT4(crypto_kem_keypair_derand)
# define crypto_kem_enc_derand VARIANT4(crypto_kem_enc_derand)
# define crypto_kem_keypair VARIANT4(crypto_kem_keypair)
# define crypto_kem_enc VARIANT4(crypto_kem_enc)
# define crypto_kem_dec VARIANT4(crypto_kem_dec)
# define polyvec VARIANT4(polyvec)
# define polyvec_compress VARIANT4(polyvec_compress)
# define polyvec_decompress VARIANT4(polyvec_decompress)
# define polyvec_tobytes VARIANT4(polyvec_tobytes)
# define polyvec_frombytes VARIANT4(polyvec_frombytes)
# define polyvec_ntt VARIANT4(polyvec_ntt)
# define polyvec_invntt_tomont VARIANT4(polyvec_invntt_tomont)
# define polyvec_basemul_acc_montgomery VARIANT4(polyvec_basemul_acc_montgomery)
# define polyvec_reduce VARIANT4(polyvec_reduce)
# define polyvec_add VARIANT4(polyvec_add)
# define pack_pk VARIANT4(pack_pk)
# define unpack_pk VARIANT4(unpack_pk)
# define pack_sk VARIANT4(pack_sk)
# define unpack_sk VARIANT4(unpack_sk)
# define pack_ciphertext VARIANT4(pack_ciphertext)
# define unpack_ciphertext VARIANT4(unpack_ciphertext)
# define gen_matrix VARIANT4(gen_matrix)
# define indcpa_keypair_derand VARIANT4(indcpa_keypair_derand)
# define indcpa_enc VARIANT4(indcpa_enc)
# define indcpa_dec VARIANT4(indcpa_dec)
# include "kyber-kdep.c"
#endif
