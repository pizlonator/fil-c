/* camellia-glue.c - Glue for the Camellia cipher
 * Copyright (C) 2007 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* I put all the libgcrypt-specific stuff in this file to keep the
   camellia.c/camellia.h files exactly as provided by NTT.  If they
   update their code, this should make it easier to bring the changes
   in. - dshaw

   There is one small change which needs to be done: Include the
   following code at the top of camellia.h: */
#if 0

/* To use Camellia with libraries it is often useful to keep the name
 * space of the library clean.  The following macro is thus useful:
 *
 *     #define CAMELLIA_EXT_SYM_PREFIX foo_
 *
 * This prefixes all external symbols with "foo_".
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef CAMELLIA_EXT_SYM_PREFIX
#define CAMELLIA_PREFIX1(x,y) x ## y
#define CAMELLIA_PREFIX2(x,y) CAMELLIA_PREFIX1(x,y)
#define CAMELLIA_PREFIX(x)    CAMELLIA_PREFIX2(CAMELLIA_EXT_SYM_PREFIX,x)
#define Camellia_Ekeygen      CAMELLIA_PREFIX(Camellia_Ekeygen)
#define Camellia_EncryptBlock CAMELLIA_PREFIX(Camellia_EncryptBlock)
#define Camellia_DecryptBlock CAMELLIA_PREFIX(Camellia_DecryptBlock)
#define camellia_decrypt128   CAMELLIA_PREFIX(camellia_decrypt128)
#define camellia_decrypt256   CAMELLIA_PREFIX(camellia_decrypt256)
#define camellia_encrypt128   CAMELLIA_PREFIX(camellia_encrypt128)
#define camellia_encrypt256   CAMELLIA_PREFIX(camellia_encrypt256)
#define camellia_setup128     CAMELLIA_PREFIX(camellia_setup128)
#define camellia_setup192     CAMELLIA_PREFIX(camellia_setup192)
#define camellia_setup256     CAMELLIA_PREFIX(camellia_setup256)
#endif /*CAMELLIA_EXT_SYM_PREFIX*/

#endif /* Code sample. */


#include <config.h>
#include "types.h"
#include "g10lib.h"
#include "cipher.h"
#include "camellia.h"
#include "bufhelp.h"
#include "cipher-internal.h"
#include "bulkhelp.h"

/* Helper macro to force alignment to 16 bytes.  */
#ifdef HAVE_GCC_ATTRIBUTE_ALIGNED
# define ATTR_ALIGNED_16  __attribute__ ((aligned (16)))
#else
# define ATTR_ALIGNED_16
#endif

/* USE_AESNI inidicates whether to compile with Intel AES-NI/AVX code. */
#undef USE_AESNI_AVX
#if defined(ENABLE_AESNI_SUPPORT) && defined(ENABLE_AVX_SUPPORT)
# if defined(__x86_64__) && (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))
#  define USE_AESNI_AVX 1
# endif
#endif

/* USE_AESNI_AVX2 inidicates whether to compile with Intel AES-NI/AVX2 code. */
#undef USE_AESNI_AVX2
#if defined(ENABLE_AESNI_SUPPORT) && defined(ENABLE_AVX2_SUPPORT)
# if defined(__x86_64__) && (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))
#  define USE_AESNI_AVX2 1
# endif
#endif

/* USE_VAES_AVX2 inidicates whether to compile with Intel VAES/AVX2 code. */
#undef USE_VAES_AVX2
#if defined(USE_AESNI_AVX2) && defined(HAVE_GCC_INLINE_ASM_VAES_VPCLMUL)
# define USE_VAES_AVX2 1
#endif

/* USE_GFNI_AVX2 inidicates whether to compile with Intel GFNI/AVX2 code. */
#undef USE_GFNI_AVX2
#if defined(USE_AESNI_AVX2) && defined(ENABLE_GFNI_SUPPORT)
# define USE_GFNI_AVX2 1
#endif

/* USE_GFNI_AVX512 inidicates whether to compile with Intel GFNI/AVX512 code. */
#undef USE_GFNI_AVX512
#if defined(USE_GFNI_AVX2) && defined(ENABLE_AVX512_SUPPORT)
# define USE_GFNI_AVX512 1
#endif

/* USE_PPC_CRYPTO indicates whether to enable PowerPC vector crypto
 * accelerated code. */
#undef USE_PPC_CRYPTO
#if !defined(WORDS_BIGENDIAN) && defined(ENABLE_PPC_CRYPTO_SUPPORT) && \
    defined(HAVE_COMPATIBLE_CC_PPC_ALTIVEC) && \
    defined(HAVE_GCC_INLINE_ASM_PPC_ALTIVEC) && \
    (SIZEOF_UNSIGNED_LONG == 8) && (__GNUC__ >= 4)
# define USE_PPC_CRYPTO 1
#endif

/* USE_AARCH64_CE indicates whether to enable ARMv8/CE accelerated code. */
#undef USE_AARCH64_CE
#if defined(__AARCH64EL__) && \
    defined(HAVE_COMPATIBLE_GCC_AARCH64_PLATFORM_AS) && \
    defined(HAVE_GCC_INLINE_ASM_AARCH64_CRYPTO) && \
    defined(HAVE_COMPATIBLE_CC_AARCH64_NEON_INTRINSICS) && \
    (__GNUC__ >= 4)
# define USE_AARCH64_CE 1
#endif

typedef struct
{
  KEY_TABLE_TYPE keytable;
  int keybitlength;
#ifdef USE_AESNI_AVX
  unsigned int use_aesni_avx:1;	/* AES-NI/AVX implementation shall be used.  */
#endif /*USE_AESNI_AVX*/
#ifdef USE_AESNI_AVX2
  unsigned int use_avx2:1; /* If any of AVX2 implementation is enabled.  */
  unsigned int use_aesni_avx2:1;/* AES-NI/AVX2 implementation shall be used.  */
  unsigned int use_vaes_avx2:1; /* VAES/AVX2 implementation shall be used.  */
  unsigned int use_gfni_avx2:1; /* GFNI/AVX2 implementation shall be used.  */
  unsigned int use_gfni_avx512:1; /* GFNI/AVX512 implementation shall be used.  */
#endif /*USE_AESNI_AVX2*/
#ifdef USE_PPC_CRYPTO
  unsigned int use_ppc:1;
  unsigned int use_ppc8:1;
  unsigned int use_ppc9:1;
#endif /*USE_PPC_CRYPTO*/
#ifdef USE_AARCH64_CE
  unsigned int use_aarch64ce:1;
#endif /*USE_AARCH64_CE*/
} CAMELLIA_context;

/* Assembly implementations use SystemV ABI, ABI conversion and additional
 * stack to store XMM6-XMM15 needed on Win64. */
#undef ASM_FUNC_ABI
#undef ASM_EXTRA_STACK
#if defined(USE_AESNI_AVX) || defined(USE_AESNI_AVX2)
# ifdef HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS
#  define ASM_FUNC_ABI __attribute__((sysv_abi))
#  define ASM_EXTRA_STACK (10 * 16)
# else
#  define ASM_FUNC_ABI
#  define ASM_EXTRA_STACK 0
# endif
#endif

#ifdef USE_AESNI_AVX
/* Assembler implementations of Camellia using AES-NI and AVX.  Process data
   in 16 blocks same time.
 */
extern void _gcry_camellia_aesni_avx_ctr_enc(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *ctr) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx_cbc_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx_cfb_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx_ocb_enc(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *offset,
					     unsigned char *checksum,
					     const u64 Ls[16]) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx_ocb_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *offset,
					     unsigned char *checksum,
					     const u64 Ls[16]) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx_ocb_auth(CAMELLIA_context *ctx,
					      const unsigned char *abuf,
					      unsigned char *offset,
					      unsigned char *checksum,
					      const u64 Ls[16]) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx_keygen(CAMELLIA_context *ctx,
					    const unsigned char *key,
					    unsigned int keylen) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx_ecb_enc(const CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in)
					     ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx_ecb_dec(const CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in)
					     ASM_FUNC_ABI;

static const int avx_burn_stack_depth = 16 * CAMELLIA_BLOCK_SIZE + 16 +
                                        2 * sizeof(void *) + ASM_EXTRA_STACK;

#endif

#ifdef USE_AESNI_AVX2
/* Assembler implementations of Camellia using AES-NI and AVX2.  Process data
   in 32 blocks same time.
 */
extern void _gcry_camellia_aesni_avx2_ctr_enc(CAMELLIA_context *ctx,
					      unsigned char *out,
					      const unsigned char *in,
					      unsigned char *ctr) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx2_cbc_dec(CAMELLIA_context *ctx,
					      unsigned char *out,
					      const unsigned char *in,
					      unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx2_cfb_dec(CAMELLIA_context *ctx,
					      unsigned char *out,
					      const unsigned char *in,
					      unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx2_ocb_enc(CAMELLIA_context *ctx,
					      unsigned char *out,
					      const unsigned char *in,
					      unsigned char *offset,
					      unsigned char *checksum,
					      const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx2_ocb_dec(CAMELLIA_context *ctx,
					      unsigned char *out,
					      const unsigned char *in,
					      unsigned char *offset,
					      unsigned char *checksum,
					      const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx2_ocb_auth(CAMELLIA_context *ctx,
					       const unsigned char *abuf,
					       unsigned char *offset,
					       unsigned char *checksum,
					       const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx2_enc_blk1_32(const CAMELLIA_context *ctx,
                                                  unsigned char *out,
                                                  const unsigned char *in,
                                                  unsigned int nblocks)
                                                  ASM_FUNC_ABI;

extern void _gcry_camellia_aesni_avx2_dec_blk1_32(const CAMELLIA_context *ctx,
                                                  unsigned char *out,
                                                  const unsigned char *in,
                                                  unsigned int nblocks)
                                                  ASM_FUNC_ABI;

static const int avx2_burn_stack_depth = 32 * CAMELLIA_BLOCK_SIZE + 16 +
                                         2 * sizeof(void *) + ASM_EXTRA_STACK;

#endif

#ifdef USE_VAES_AVX2
/* Assembler implementations of Camellia using VAES and AVX2.  Process data
   in 32 blocks same time.
 */
extern void _gcry_camellia_vaes_avx2_ctr_enc(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *ctr) ASM_FUNC_ABI;

extern void _gcry_camellia_vaes_avx2_cbc_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_vaes_avx2_cfb_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_vaes_avx2_ocb_enc(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *offset,
					     unsigned char *checksum,
					     const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_vaes_avx2_ocb_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *offset,
					     unsigned char *checksum,
					     const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_vaes_avx2_ocb_auth(CAMELLIA_context *ctx,
					      const unsigned char *abuf,
					      unsigned char *offset,
					      unsigned char *checksum,
					      const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_vaes_avx2_enc_blk1_32(const CAMELLIA_context *ctx,
                                                 unsigned char *out,
                                                 const unsigned char *in,
                                                 unsigned int nblocks)
                                                 ASM_FUNC_ABI;

extern void _gcry_camellia_vaes_avx2_dec_blk1_32(const CAMELLIA_context *ctx,
                                                 unsigned char *out,
                                                 const unsigned char *in,
                                                 unsigned int nblocks)
                                                 ASM_FUNC_ABI;
#endif

#ifdef USE_GFNI_AVX2
/* Assembler implementations of Camellia using GFNI and AVX2.  Process data
   in 32 blocks same time.
 */
extern void _gcry_camellia_gfni_avx2_ctr_enc(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *ctr) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx2_cbc_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx2_cfb_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx2_ocb_enc(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *offset,
					     unsigned char *checksum,
					     const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx2_ocb_dec(CAMELLIA_context *ctx,
					     unsigned char *out,
					     const unsigned char *in,
					     unsigned char *offset,
					     unsigned char *checksum,
					     const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx2_ocb_auth(CAMELLIA_context *ctx,
					      const unsigned char *abuf,
					      unsigned char *offset,
					      unsigned char *checksum,
					      const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx2_enc_blk1_32(const CAMELLIA_context *ctx,
                                                 unsigned char *out,
                                                 const unsigned char *in,
                                                 unsigned int nblocks)
                                                 ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx2_dec_blk1_32(const CAMELLIA_context *ctx,
                                                 unsigned char *out,
                                                 const unsigned char *in,
                                                 unsigned int nblocks)
                                                 ASM_FUNC_ABI;
#endif

#ifdef USE_GFNI_AVX512
/* Assembler implementations of Camellia using GFNI and AVX512.  Process data
   in 64 blocks same time.
 */
extern void _gcry_camellia_gfni_avx512_ctr_enc(CAMELLIA_context *ctx,
                                               unsigned char *out,
                                               const unsigned char *in,
                                               unsigned char *ctr) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx512_cbc_dec(CAMELLIA_context *ctx,
                                               unsigned char *out,
                                               const unsigned char *in,
                                               unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx512_cfb_dec(CAMELLIA_context *ctx,
                                               unsigned char *out,
                                               const unsigned char *in,
                                               unsigned char *iv) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx512_ocb_enc(CAMELLIA_context *ctx,
                                               unsigned char *out,
                                               const unsigned char *in,
                                               unsigned char *offset,
                                               unsigned char *checksum,
                                               const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx512_ocb_dec(CAMELLIA_context *ctx,
                                               unsigned char *out,
                                               const unsigned char *in,
                                               unsigned char *offset,
                                               unsigned char *checksum,
                                               const u64 Ls[32]) ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx512_enc_blk64(const CAMELLIA_context *ctx,
                                                 unsigned char *out,
                                                 const unsigned char *in)
                                                 ASM_FUNC_ABI;

extern void _gcry_camellia_gfni_avx512_dec_blk64(const CAMELLIA_context *ctx,
                                                 unsigned char *out,
                                                 const unsigned char *in)
                                                 ASM_FUNC_ABI;

/* Stack not used by AVX512 implementation. */
static const int avx512_burn_stack_depth = 0;
#endif

#ifdef USE_PPC_CRYPTO
extern void _gcry_camellia_ppc8_encrypt_blk16(const void *key_table,
					      void *out,
					      const void *in,
					      int key_length);

extern void _gcry_camellia_ppc8_decrypt_blk16(const void *key_table,
					      void *out,
					      const void *in,
					      int key_length);

extern void _gcry_camellia_ppc9_encrypt_blk16(const void *key_table,
					      void *out,
					      const void *in,
					      int key_length);

extern void _gcry_camellia_ppc9_decrypt_blk16(const void *key_table,
					      void *out,
					      const void *in,
					      int key_length);

extern void _gcry_camellia_ppc8_keygen(void *key_table, const void *vkey,
				       unsigned int keylen);

extern void _gcry_camellia_ppc9_keygen(void *key_table, const void *vkey,
				       unsigned int keylen);

void camellia_ppc_enc_blk16(const CAMELLIA_context *ctx, unsigned char *out,
                            const unsigned char *in)
{
  if (ctx->use_ppc9)
    _gcry_camellia_ppc9_encrypt_blk16 (ctx->keytable, out, in,
				       ctx->keybitlength / 8);
  else
    _gcry_camellia_ppc8_encrypt_blk16 (ctx->keytable, out, in,
				       ctx->keybitlength / 8);
}

void camellia_ppc_dec_blk16(const CAMELLIA_context *ctx, unsigned char *out,
                            const unsigned char *in)
{
  if (ctx->use_ppc9)
    _gcry_camellia_ppc9_decrypt_blk16 (ctx->keytable, out, in,
				       ctx->keybitlength / 8);
  else
    _gcry_camellia_ppc8_decrypt_blk16 (ctx->keytable, out, in,
				       ctx->keybitlength / 8);
}

static const int ppc_burn_stack_depth = 16 * CAMELLIA_BLOCK_SIZE + 16 +
                                        2 * sizeof(void *);
#endif /*USE_PPC_CRYPTO*/

#ifdef USE_AARCH64_CE
extern void _gcry_camellia_aarch64ce_encrypt_blk16(const void *key_table,
						   void *out, const void *in,
						   int key_length);

extern void _gcry_camellia_aarch64ce_decrypt_blk16(const void *key_table,
						   void *out, const void *in,
						   int key_length);

extern void _gcry_camellia_aarch64ce_keygen(void *key_table, const void *vkey,
					    unsigned int keylen);

void camellia_aarch64ce_enc_blk16(const CAMELLIA_context *ctx,
				  unsigned char *out, const unsigned char *in)
{
  _gcry_camellia_aarch64ce_encrypt_blk16 (ctx->keytable, out, in,
					  ctx->keybitlength / 8);
}

void camellia_aarch64ce_dec_blk16(const CAMELLIA_context *ctx,
				  unsigned char *out, const unsigned char *in)
{
  _gcry_camellia_aarch64ce_decrypt_blk16 (ctx->keytable, out, in,
					  ctx->keybitlength / 8);
}

static const int aarch64ce_burn_stack_depth = 16 * CAMELLIA_BLOCK_SIZE + 16 +
					      2 * sizeof(void *);
#endif /*USE_AARCH64_CE*/

static const char *selftest(void);

static void _gcry_camellia_ctr_enc (void *context, unsigned char *ctr,
				    void *outbuf_arg, const void *inbuf_arg,
				    size_t nblocks);
static void _gcry_camellia_cbc_dec (void *context, unsigned char *iv,
				    void *outbuf_arg, const void *inbuf_arg,
				    size_t nblocks);
static void _gcry_camellia_cfb_dec (void *context, unsigned char *iv,
				    void *outbuf_arg, const void *inbuf_arg,
				    size_t nblocks);
static void _gcry_camellia_xts_crypt (void *context, unsigned char *tweak,
				      void *outbuf_arg, const void *inbuf_arg,
				      size_t nblocks, int encrypt);
static void _gcry_camellia_ecb_crypt (void *context, void *outbuf_arg,
				      const void *inbuf_arg, size_t nblocks,
				      int encrypt);
static void _gcry_camellia_ctr32le_enc (void *context, unsigned char *ctr,
					void *outbuf_arg, const void *inbuf_arg,
					size_t nblocks);
static size_t _gcry_camellia_ocb_crypt (gcry_cipher_hd_t c, void *outbuf_arg,
					const void *inbuf_arg, size_t nblocks,
					int encrypt);
static size_t _gcry_camellia_ocb_auth (gcry_cipher_hd_t c, const void *abuf_arg,
				       size_t nblocks);

static gcry_err_code_t
camellia_setkey(void *c, const byte *key, unsigned keylen,
                cipher_bulk_ops_t *bulk_ops)
{
  CAMELLIA_context *ctx=c;
  static int initialized=0;
  static const char *selftest_failed=NULL;
  unsigned int hwf = _gcry_get_hw_features ();

  (void)hwf;

  if(keylen!=16 && keylen!=24 && keylen!=32)
    return GPG_ERR_INV_KEYLEN;

  if(!initialized)
    {
      initialized=1;
      selftest_failed=selftest();
      if(selftest_failed)
	log_error("%s\n",selftest_failed);
    }

  if(selftest_failed)
    return GPG_ERR_SELFTEST_FAILED;

#ifdef USE_AESNI_AVX
  ctx->use_aesni_avx = (hwf & HWF_INTEL_AESNI) && (hwf & HWF_INTEL_AVX);
#endif
#ifdef USE_AESNI_AVX2
  ctx->use_aesni_avx2 = (hwf & HWF_INTEL_AESNI) && (hwf & HWF_INTEL_AVX2);
  ctx->use_vaes_avx2 = 0;
  ctx->use_gfni_avx2 = 0;
  ctx->use_gfni_avx512 = 0;
  ctx->use_avx2 = ctx->use_aesni_avx2;
#endif
#ifdef USE_VAES_AVX2
  ctx->use_vaes_avx2 = (hwf & HWF_INTEL_VAES_VPCLMUL) && (hwf & HWF_INTEL_AVX2);
  ctx->use_avx2 |= ctx->use_vaes_avx2;
#endif
#ifdef USE_GFNI_AVX2
  ctx->use_gfni_avx2 = (hwf & HWF_INTEL_GFNI) && (hwf & HWF_INTEL_AVX2);
  ctx->use_avx2 |= ctx->use_gfni_avx2;
#endif
#ifdef USE_GFNI_AVX512
  ctx->use_gfni_avx512 = (hwf & HWF_INTEL_GFNI) && (hwf & HWF_INTEL_AVX512);
#endif
#ifdef USE_PPC_CRYPTO
  ctx->use_ppc8 = (hwf & HWF_PPC_VCRYPTO) != 0;
  ctx->use_ppc9 = (hwf & HWF_PPC_VCRYPTO) && (hwf & HWF_PPC_ARCH_3_00);
  ctx->use_ppc = ctx->use_ppc8 || ctx->use_ppc9;
#endif
#ifdef USE_AARCH64_CE
  ctx->use_aarch64ce = (hwf & HWF_ARM_AES) != 0;
#endif

  ctx->keybitlength=keylen*8;

  /* Setup bulk encryption routines.  */
  memset (bulk_ops, 0, sizeof(*bulk_ops));
  bulk_ops->cbc_dec = _gcry_camellia_cbc_dec;
  bulk_ops->cfb_dec = _gcry_camellia_cfb_dec;
  bulk_ops->ctr_enc = _gcry_camellia_ctr_enc;
  bulk_ops->ocb_crypt = _gcry_camellia_ocb_crypt;
  bulk_ops->ocb_auth  = _gcry_camellia_ocb_auth;
  bulk_ops->xts_crypt = _gcry_camellia_xts_crypt;
  bulk_ops->ecb_crypt = _gcry_camellia_ecb_crypt;
  bulk_ops->ctr32le_enc = _gcry_camellia_ctr32le_enc;

  if (0)
    { }
#ifdef USE_AESNI_AVX
  else if (ctx->use_aesni_avx)
    _gcry_camellia_aesni_avx_keygen(ctx, key, keylen);
#endif
#ifdef USE_PPC_CRYPTO
  else if (ctx->use_ppc9)
    _gcry_camellia_ppc9_keygen(ctx->keytable, key, keylen);
  else if (ctx->use_ppc8)
    _gcry_camellia_ppc8_keygen(ctx->keytable, key, keylen);
#endif
#ifdef USE_AARCH64_CE
  else if (ctx->use_aarch64ce)
    _gcry_camellia_aarch64ce_keygen(ctx->keytable, key, keylen);
#endif
  else
    {
      Camellia_Ekeygen(ctx->keybitlength,key,ctx->keytable);
      _gcry_burn_stack
        ((19+34+34)*sizeof(u32)+2*sizeof(void*) /* camellia_setup256 */
         +(4+32)*sizeof(u32)+2*sizeof(void*)    /* camellia_setup192 */
         +0+sizeof(int)+2*sizeof(void*)         /* Camellia_Ekeygen */
         +3*2*sizeof(void*)                     /* Function calls.  */
         );
    }

#ifdef USE_GFNI_AVX2
  if (ctx->use_gfni_avx2)
    {
      /* Disable AESNI & VAES implementations when GFNI implementation is
       * enabled. */
#ifdef USE_AESNI_AVX
      ctx->use_aesni_avx = 0;
#endif
#ifdef USE_AESNI_AVX2
      ctx->use_aesni_avx2 = 0;
#endif
#ifdef USE_VAES_AVX2
      ctx->use_vaes_avx2 = 0;
#endif
    }
#endif

  return 0;
}

#ifdef USE_ARM_ASM

/* Assembly implementations of Camellia. */
extern void _gcry_camellia_arm_encrypt_block(const KEY_TABLE_TYPE keyTable,
					       byte *outbuf, const byte *inbuf,
					       const int keybits);

extern void _gcry_camellia_arm_decrypt_block(const KEY_TABLE_TYPE keyTable,
					       byte *outbuf, const byte *inbuf,
					       const int keybits);

static void Camellia_EncryptBlock(const int keyBitLength,
				  const unsigned char *plaintext,
				  const KEY_TABLE_TYPE keyTable,
				  unsigned char *cipherText)
{
  _gcry_camellia_arm_encrypt_block(keyTable, cipherText, plaintext,
				     keyBitLength);
}

static void Camellia_DecryptBlock(const int keyBitLength,
				  const unsigned char *cipherText,
				  const KEY_TABLE_TYPE keyTable,
				  unsigned char *plaintext)
{
  _gcry_camellia_arm_decrypt_block(keyTable, plaintext, cipherText,
				     keyBitLength);
}

#ifdef __aarch64__
#  define CAMELLIA_encrypt_stack_burn_size (0)
#  define CAMELLIA_decrypt_stack_burn_size (0)
#else
#  define CAMELLIA_encrypt_stack_burn_size (15*4)
#  define CAMELLIA_decrypt_stack_burn_size (15*4)
#endif

static unsigned int
camellia_encrypt(void *c, byte *outbuf, const byte *inbuf)
{
  CAMELLIA_context *ctx = c;
  Camellia_EncryptBlock(ctx->keybitlength,inbuf,ctx->keytable,outbuf);
  return /*burn_stack*/ (CAMELLIA_encrypt_stack_burn_size);
}

static unsigned int
camellia_decrypt(void *c, byte *outbuf, const byte *inbuf)
{
  CAMELLIA_context *ctx=c;
  Camellia_DecryptBlock(ctx->keybitlength,inbuf,ctx->keytable,outbuf);
  return /*burn_stack*/ (CAMELLIA_decrypt_stack_burn_size);
}

#else /*USE_ARM_ASM*/

static unsigned int
camellia_encrypt(void *c, byte *outbuf, const byte *inbuf)
{
  CAMELLIA_context *ctx=c;

  Camellia_EncryptBlock(ctx->keybitlength,inbuf,ctx->keytable,outbuf);

#define CAMELLIA_encrypt_stack_burn_size \
  (sizeof(int)+2*sizeof(unsigned char *)+sizeof(void*/*KEY_TABLE_TYPE*/) \
     +4*sizeof(u32)+4*sizeof(u32) \
     +2*sizeof(u32*)+4*sizeof(u32) \
     +2*2*sizeof(void*) /* Function calls.  */ \
    )

  return /*burn_stack*/ (CAMELLIA_encrypt_stack_burn_size);
}

static unsigned int
camellia_decrypt(void *c, byte *outbuf, const byte *inbuf)
{
  CAMELLIA_context *ctx=c;

  Camellia_DecryptBlock(ctx->keybitlength,inbuf,ctx->keytable,outbuf);

#define CAMELLIA_decrypt_stack_burn_size \
    (sizeof(int)+2*sizeof(unsigned char *)+sizeof(void*/*KEY_TABLE_TYPE*/) \
     +4*sizeof(u32)+4*sizeof(u32) \
     +2*sizeof(u32*)+4*sizeof(u32) \
     +2*2*sizeof(void*) /* Function calls.  */ \
    )

  return /*burn_stack*/ (CAMELLIA_decrypt_stack_burn_size);
}

#endif /*!USE_ARM_ASM*/


static unsigned int
camellia_encrypt_blk1_32 (void *priv, byte *outbuf, const byte *inbuf,
			  size_t num_blks)
{
  const CAMELLIA_context *ctx = priv;
  unsigned int stack_burn_size = 0;

  gcry_assert (num_blks <= 32);

#ifdef USE_GFNI_AVX2
  if (ctx->use_gfni_avx2 && num_blks >= 2)
    {
      /* 2 or more parallel block GFNI processing is faster than
       * generic C implementation.  */
      _gcry_camellia_gfni_avx2_enc_blk1_32 (ctx, outbuf, inbuf, num_blks);
      return avx2_burn_stack_depth;
    }
#endif
#ifdef USE_VAES_AVX2
  if (ctx->use_vaes_avx2 && num_blks >= 4)
    {
      /* 4 or more parallel block VAES processing is faster than
       * generic C implementation.  */
      _gcry_camellia_vaes_avx2_enc_blk1_32 (ctx, outbuf, inbuf, num_blks);
      return avx2_burn_stack_depth;
    }
#endif
#ifdef USE_AESNI_AVX2
  if (ctx->use_aesni_avx2 && num_blks >= 5)
    {
      /* 5 or more parallel block AESNI processing is faster than
       * generic C implementation.  */
      _gcry_camellia_aesni_avx2_enc_blk1_32 (ctx, outbuf, inbuf, num_blks);
      return avx2_burn_stack_depth;
    }
#endif
#ifdef USE_AESNI_AVX
  while (ctx->use_aesni_avx && num_blks >= 16)
    {
      _gcry_camellia_aesni_avx_ecb_enc (ctx, outbuf, inbuf);
      stack_burn_size = avx_burn_stack_depth;
      outbuf += CAMELLIA_BLOCK_SIZE * 16;
      inbuf += CAMELLIA_BLOCK_SIZE * 16;
      num_blks -= 16;
    }
#endif
#ifdef USE_PPC_CRYPTO
  while (ctx->use_ppc && num_blks >= 16)
    {
      camellia_ppc_enc_blk16 (ctx, outbuf, inbuf);
      stack_burn_size = ppc_burn_stack_depth;
      outbuf += CAMELLIA_BLOCK_SIZE * 16;
      inbuf += CAMELLIA_BLOCK_SIZE * 16;
      num_blks -= 16;
    }
#endif
#ifdef USE_AARCH64_CE
  while (ctx->use_aarch64ce && num_blks >= 16)
    {
      camellia_aarch64ce_enc_blk16 (ctx, outbuf, inbuf);
      stack_burn_size = aarch64ce_burn_stack_depth;
      outbuf += CAMELLIA_BLOCK_SIZE * 16;
      inbuf += CAMELLIA_BLOCK_SIZE * 16;
      num_blks -= 16;
    }
#endif

  while (num_blks)
    {
      unsigned int nburn = camellia_encrypt((void *)ctx, outbuf, inbuf);
      stack_burn_size = nburn > stack_burn_size ? nburn : stack_burn_size;
      outbuf += CAMELLIA_BLOCK_SIZE;
      inbuf += CAMELLIA_BLOCK_SIZE;
      num_blks--;
    }

  return stack_burn_size;
}

static unsigned int
camellia_encrypt_blk1_64 (void *priv, byte *outbuf, const byte *inbuf,
			  size_t num_blks)
{
  CAMELLIA_context *ctx = priv;
  unsigned int stack_burn_size = 0;
  unsigned int nburn;

  gcry_assert (num_blks <= 64);

#ifdef USE_GFNI_AVX512
  if (num_blks == 64 && ctx->use_gfni_avx512)
    {
      _gcry_camellia_gfni_avx512_enc_blk64 (ctx, outbuf, inbuf);
      return avx512_burn_stack_depth;
    }
#endif

  do
    {
      unsigned int curr_blks = num_blks > 32 ? 32 : num_blks;
      nburn = camellia_encrypt_blk1_32 (ctx, outbuf, inbuf, curr_blks);
      stack_burn_size = nburn > stack_burn_size ? nburn : stack_burn_size;
      outbuf += curr_blks * 16;
      inbuf += curr_blks * 16;
      num_blks -= curr_blks;
    }
  while (num_blks > 0);

  return stack_burn_size;
}

static unsigned int
camellia_decrypt_blk1_32 (void *priv, byte *outbuf, const byte *inbuf,
			  size_t num_blks)
{
  const CAMELLIA_context *ctx = priv;
  unsigned int stack_burn_size = 0;

  gcry_assert (num_blks <= 32);

#ifdef USE_GFNI_AVX2
  if (ctx->use_gfni_avx2 && num_blks >= 2)
    {
      /* 2 or more parallel block GFNI processing is faster than
       * generic C implementation.  */
      _gcry_camellia_gfni_avx2_dec_blk1_32 (ctx, outbuf, inbuf, num_blks);
      return avx2_burn_stack_depth;
    }
#endif
#ifdef USE_VAES_AVX2
  if (ctx->use_vaes_avx2 && num_blks >= 4)
    {
      /* 4 or more parallel block VAES processing is faster than
       * generic C implementation.  */
      _gcry_camellia_vaes_avx2_dec_blk1_32 (ctx, outbuf, inbuf, num_blks);
      return avx2_burn_stack_depth;
    }
#endif
#ifdef USE_AESNI_AVX2
  if (ctx->use_aesni_avx2 && num_blks >= 5)
    {
      /* 5 or more parallel block AESNI processing is faster than
       * generic C implementation.  */
      _gcry_camellia_aesni_avx2_dec_blk1_32 (ctx, outbuf, inbuf, num_blks);
      return avx2_burn_stack_depth;
    }
#endif
#ifdef USE_AESNI_AVX
  while (ctx->use_aesni_avx && num_blks >= 16)
    {
      _gcry_camellia_aesni_avx_ecb_dec (ctx, outbuf, inbuf);
      stack_burn_size = avx_burn_stack_depth;
      outbuf += CAMELLIA_BLOCK_SIZE * 16;
      inbuf += CAMELLIA_BLOCK_SIZE * 16;
      num_blks -= 16;
    }
#endif
#ifdef USE_PPC_CRYPTO
  while (ctx->use_ppc && num_blks >= 16)
    {
      camellia_ppc_dec_blk16 (ctx, outbuf, inbuf);
      stack_burn_size = ppc_burn_stack_depth;
      outbuf += CAMELLIA_BLOCK_SIZE * 16;
      inbuf += CAMELLIA_BLOCK_SIZE * 16;
      num_blks -= 16;
    }
#endif
#ifdef USE_AARCH64_CE
  while (ctx->use_aarch64ce && num_blks >= 16)
    {
      camellia_aarch64ce_dec_blk16 (ctx, outbuf, inbuf);
      stack_burn_size = aarch64ce_burn_stack_depth;
      outbuf += CAMELLIA_BLOCK_SIZE * 16;
      inbuf += CAMELLIA_BLOCK_SIZE * 16;
      num_blks -= 16;
    }
#endif

  while (num_blks)
    {
      unsigned int nburn = camellia_decrypt((void *)ctx, outbuf, inbuf);
      stack_burn_size = nburn > stack_burn_size ? nburn : stack_burn_size;
      outbuf += CAMELLIA_BLOCK_SIZE;
      inbuf += CAMELLIA_BLOCK_SIZE;
      num_blks--;
    }

  return stack_burn_size;
}

static unsigned int
camellia_decrypt_blk1_64 (void *priv, byte *outbuf, const byte *inbuf,
			  size_t num_blks)
{
  CAMELLIA_context *ctx = priv;
  unsigned int stack_burn_size = 0;
  unsigned int nburn;

  gcry_assert (num_blks <= 64);

#ifdef USE_GFNI_AVX512
  if (num_blks == 64 && ctx->use_gfni_avx512)
    {
      _gcry_camellia_gfni_avx512_dec_blk64 (ctx, outbuf, inbuf);
      return avx512_burn_stack_depth;
    }
#endif

  do
    {
      unsigned int curr_blks = num_blks > 32 ? 32 : num_blks;
      nburn = camellia_decrypt_blk1_32 (ctx, outbuf, inbuf, curr_blks);
      stack_burn_size = nburn > stack_burn_size ? nburn : stack_burn_size;
      outbuf += curr_blks * 16;
      inbuf += curr_blks * 16;
      num_blks -= curr_blks;
    }
  while (num_blks > 0);

  return stack_burn_size;
}


/* Bulk encryption of complete blocks in CTR mode.  This function is only
   intended for the bulk encryption feature of cipher.c.  CTR is expected to be
   of size CAMELLIA_BLOCK_SIZE. */
static void
_gcry_camellia_ctr_enc(void *context, unsigned char *ctr,
                       void *outbuf_arg, const void *inbuf_arg,
                       size_t nblocks)
{
  CAMELLIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

#ifdef USE_GFNI_AVX512
  if (ctx->use_gfni_avx512)
    {
      int did_use_gfni_avx512 = 0;

      /* Process data in 64 block chunks. */
      while (nblocks >= 64)
        {
          _gcry_camellia_gfni_avx512_ctr_enc (ctx, outbuf, inbuf, ctr);
          nblocks -= 64;
          outbuf += 64 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 64 * CAMELLIA_BLOCK_SIZE;
          did_use_gfni_avx512 = 1;
        }

      if (did_use_gfni_avx512)
        {
          if (burn_stack_depth < avx512_burn_stack_depth)
            burn_stack_depth = avx512_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX2
  if (ctx->use_avx2)
    {
      int did_use_aesni_avx2 = 0;
      typeof (&_gcry_camellia_aesni_avx2_ctr_enc) bulk_ctr_fn =
	  _gcry_camellia_aesni_avx2_ctr_enc;

#ifdef USE_VAES_AVX2
      if (ctx->use_vaes_avx2)
	bulk_ctr_fn =_gcry_camellia_vaes_avx2_ctr_enc;
#endif
#ifdef USE_GFNI_AVX2
      if (ctx->use_gfni_avx2)
	bulk_ctr_fn =_gcry_camellia_gfni_avx2_ctr_enc;
#endif

      /* Process data in 32 block chunks. */
      while (nblocks >= 32)
        {
	  bulk_ctr_fn (ctx, outbuf, inbuf, ctr);
          nblocks -= 32;
          outbuf += 32 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 32 * CAMELLIA_BLOCK_SIZE;
          did_use_aesni_avx2 = 1;
        }

      if (did_use_aesni_avx2)
        {
          if (burn_stack_depth < avx2_burn_stack_depth)
            burn_stack_depth = avx2_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX
  if (ctx->use_aesni_avx)
    {
      int did_use_aesni_avx = 0;

      /* Process data in 16 block chunks. */
      while (nblocks >= 16)
        {
          _gcry_camellia_aesni_avx_ctr_enc(ctx, outbuf, inbuf, ctr);

          nblocks -= 16;
          outbuf += 16 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 16 * CAMELLIA_BLOCK_SIZE;
          did_use_aesni_avx = 1;
        }

      if (did_use_aesni_avx)
        {
          if (burn_stack_depth < avx_burn_stack_depth)
            burn_stack_depth = avx_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

  /* Process remaining blocks. */
  if (nblocks)
    {
      byte tmpbuf[CAMELLIA_BLOCK_SIZE * 32];
      unsigned int tmp_used = CAMELLIA_BLOCK_SIZE;
      size_t nburn;

      nburn = bulk_ctr_enc_128(ctx, camellia_encrypt_blk1_32, outbuf, inbuf,
                               nblocks, ctr, tmpbuf,
                               sizeof(tmpbuf) / CAMELLIA_BLOCK_SIZE, &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory(tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack(burn_stack_depth);
}

/* Bulk decryption of complete blocks in CBC mode.  This function is only
   intended for the bulk encryption feature of cipher.c. */
static void
_gcry_camellia_cbc_dec(void *context, unsigned char *iv,
                       void *outbuf_arg, const void *inbuf_arg,
                       size_t nblocks)
{
  CAMELLIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

#ifdef USE_GFNI_AVX512
  if (ctx->use_gfni_avx512)
    {
      int did_use_gfni_avx512 = 0;

      /* Process data in 64 block chunks. */
      while (nblocks >= 64)
        {
          _gcry_camellia_gfni_avx512_cbc_dec (ctx, outbuf, inbuf, iv);
          nblocks -= 64;
          outbuf += 64 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 64 * CAMELLIA_BLOCK_SIZE;
          did_use_gfni_avx512 = 1;
        }

      if (did_use_gfni_avx512)
        {
          if (burn_stack_depth < avx512_burn_stack_depth)
            burn_stack_depth = avx512_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX2
  if (ctx->use_avx2)
    {
      int did_use_aesni_avx2 = 0;
      typeof (&_gcry_camellia_aesni_avx2_cbc_dec) bulk_cbc_fn =
	  _gcry_camellia_aesni_avx2_cbc_dec;

#ifdef USE_VAES_AVX2
      if (ctx->use_vaes_avx2)
	bulk_cbc_fn =_gcry_camellia_vaes_avx2_cbc_dec;
#endif
#ifdef USE_GFNI_AVX2
      if (ctx->use_gfni_avx2)
	bulk_cbc_fn =_gcry_camellia_gfni_avx2_cbc_dec;
#endif

      /* Process data in 32 block chunks. */
      while (nblocks >= 32)
        {
	  bulk_cbc_fn (ctx, outbuf, inbuf, iv);
          nblocks -= 32;
          outbuf += 32 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 32 * CAMELLIA_BLOCK_SIZE;
          did_use_aesni_avx2 = 1;
        }

      if (did_use_aesni_avx2)
        {
          if (burn_stack_depth < avx2_burn_stack_depth)
            burn_stack_depth = avx2_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX
  if (ctx->use_aesni_avx)
    {
      int did_use_aesni_avx = 0;

      /* Process data in 16 block chunks. */
      while (nblocks >= 16)
        {
          _gcry_camellia_aesni_avx_cbc_dec(ctx, outbuf, inbuf, iv);

          nblocks -= 16;
          outbuf += 16 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 16 * CAMELLIA_BLOCK_SIZE;
          did_use_aesni_avx = 1;
        }

      if (did_use_aesni_avx)
        {
          if (burn_stack_depth < avx_burn_stack_depth)
            burn_stack_depth = avx_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

  /* Process remaining blocks. */
  if (nblocks)
    {
      byte tmpbuf[CAMELLIA_BLOCK_SIZE * 32];
      unsigned int tmp_used = CAMELLIA_BLOCK_SIZE;
      size_t nburn;

      nburn = bulk_cbc_dec_128(ctx, camellia_decrypt_blk1_32, outbuf, inbuf,
                               nblocks, iv, tmpbuf,
                               sizeof(tmpbuf) / CAMELLIA_BLOCK_SIZE, &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory(tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack(burn_stack_depth);
}

/* Bulk decryption of complete blocks in CFB mode.  This function is only
   intended for the bulk encryption feature of cipher.c. */
static void
_gcry_camellia_cfb_dec(void *context, unsigned char *iv,
                       void *outbuf_arg, const void *inbuf_arg,
                       size_t nblocks)
{
  CAMELLIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

#ifdef USE_GFNI_AVX512
  if (ctx->use_gfni_avx512)
    {
      int did_use_gfni_avx512 = 0;

      /* Process data in 64 block chunks. */
      while (nblocks >= 64)
        {
          _gcry_camellia_gfni_avx512_cfb_dec (ctx, outbuf, inbuf, iv);
          nblocks -= 64;
          outbuf += 64 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 64 * CAMELLIA_BLOCK_SIZE;
          did_use_gfni_avx512 = 1;
        }

      if (did_use_gfni_avx512)
        {
          if (burn_stack_depth < avx512_burn_stack_depth)
            burn_stack_depth = avx512_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX2
  if (ctx->use_avx2)
    {
      int did_use_aesni_avx2 = 0;
      typeof (&_gcry_camellia_aesni_avx2_cfb_dec) bulk_cfb_fn =
	  _gcry_camellia_aesni_avx2_cfb_dec;

#ifdef USE_VAES_AVX2
      if (ctx->use_vaes_avx2)
	bulk_cfb_fn =_gcry_camellia_vaes_avx2_cfb_dec;
#endif
#ifdef USE_GFNI_AVX2
      if (ctx->use_gfni_avx2)
	bulk_cfb_fn =_gcry_camellia_gfni_avx2_cfb_dec;
#endif

      /* Process data in 32 block chunks. */
      while (nblocks >= 32)
        {
	  bulk_cfb_fn (ctx, outbuf, inbuf, iv);
          nblocks -= 32;
          outbuf += 32 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 32 * CAMELLIA_BLOCK_SIZE;
          did_use_aesni_avx2 = 1;
        }

      if (did_use_aesni_avx2)
        {
          if (burn_stack_depth < avx2_burn_stack_depth)
            burn_stack_depth = avx2_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX
  if (ctx->use_aesni_avx)
    {
      int did_use_aesni_avx = 0;

      /* Process data in 16 block chunks. */
      while (nblocks >= 16)
        {
          _gcry_camellia_aesni_avx_cfb_dec(ctx, outbuf, inbuf, iv);

          nblocks -= 16;
          outbuf += 16 * CAMELLIA_BLOCK_SIZE;
          inbuf  += 16 * CAMELLIA_BLOCK_SIZE;
          did_use_aesni_avx = 1;
        }

      if (did_use_aesni_avx)
        {
          if (burn_stack_depth < avx_burn_stack_depth)
            burn_stack_depth = avx_burn_stack_depth;
        }

      /* Use generic code to handle smaller chunks... */
    }
#endif

  /* Process remaining blocks. */
  if (nblocks)
    {
      byte tmpbuf[CAMELLIA_BLOCK_SIZE * 32];
      unsigned int tmp_used = CAMELLIA_BLOCK_SIZE;
      size_t nburn;

      nburn = bulk_cfb_dec_128(ctx, camellia_encrypt_blk1_32, outbuf, inbuf,
                               nblocks, iv, tmpbuf,
                               sizeof(tmpbuf) / CAMELLIA_BLOCK_SIZE, &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory(tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack(burn_stack_depth);
}

/* Bulk encryption/decryption in ECB mode. */
static void
_gcry_camellia_ecb_crypt (void *context, void *outbuf_arg,
			  const void *inbuf_arg, size_t nblocks, int encrypt)
{
  CAMELLIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

  /* Process remaining blocks. */
  if (nblocks)
    {
      size_t nburn;

      nburn = bulk_ecb_crypt_128(ctx, encrypt ? camellia_encrypt_blk1_64
                                              : camellia_decrypt_blk1_64,
                                 outbuf, inbuf, nblocks, 64);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;
    }

  if (burn_stack_depth)
    _gcry_burn_stack(burn_stack_depth);
}

/* Bulk encryption/decryption of complete blocks in XTS mode. */
static void
_gcry_camellia_xts_crypt (void *context, unsigned char *tweak,
                          void *outbuf_arg, const void *inbuf_arg,
                          size_t nblocks, int encrypt)
{
  CAMELLIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

  /* Process remaining blocks. */
  if (nblocks)
    {
      byte tmpbuf[CAMELLIA_BLOCK_SIZE * 64];
      unsigned int tmp_used = CAMELLIA_BLOCK_SIZE;
      size_t nburn;

      nburn = bulk_xts_crypt_128(ctx, encrypt ? camellia_encrypt_blk1_64
                                              : camellia_decrypt_blk1_64,
                                 outbuf, inbuf, nblocks, tweak, tmpbuf,
                                 sizeof(tmpbuf) / CAMELLIA_BLOCK_SIZE,
                                 &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory(tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack(burn_stack_depth);
}

/* Bulk encryption of complete blocks in CTR32LE mode (for GCM-SIV). */
static void
_gcry_camellia_ctr32le_enc(void *context, unsigned char *ctr,
                           void *outbuf_arg, const void *inbuf_arg,
                           size_t nblocks)
{
  CAMELLIA_context *ctx = context;
  byte *outbuf = outbuf_arg;
  const byte *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

  /* Process remaining blocks. */
  if (nblocks)
    {
      byte tmpbuf[64 * CAMELLIA_BLOCK_SIZE];
      unsigned int tmp_used = CAMELLIA_BLOCK_SIZE;
      size_t nburn;

      nburn = bulk_ctr32le_enc_128 (ctx, camellia_encrypt_blk1_64, outbuf,
                                    inbuf, nblocks, ctr, tmpbuf,
                                    sizeof(tmpbuf) / CAMELLIA_BLOCK_SIZE,
                                    &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory (tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);
}

/* Bulk encryption/decryption of complete blocks in OCB mode. */
static size_t
_gcry_camellia_ocb_crypt (gcry_cipher_hd_t c, void *outbuf_arg,
			  const void *inbuf_arg, size_t nblocks, int encrypt)
{
#if defined(USE_PPC_CRYPTO) || defined(USE_AESNI_AVX) || defined(USE_AESNI_AVX2)
  CAMELLIA_context *ctx = (void *)&c->context.c;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;
  u64 blkn = c->u_mode.ocb.data_nblocks;

#else
  (void)c;
  (void)outbuf_arg;
  (void)inbuf_arg;
  (void)encrypt;
#endif

#ifdef USE_GFNI_AVX512
  if (ctx->use_gfni_avx512)
    {
      int did_use_gfni_avx512 = 0;
      u64 Ls[64];
      u64 *l;

      if (nblocks >= 64)
	{
	  typeof (&_gcry_camellia_gfni_avx512_ocb_dec) bulk_ocb_fn =
	      encrypt ? _gcry_camellia_gfni_avx512_ocb_enc
		      : _gcry_camellia_gfni_avx512_ocb_dec;
          l = bulk_ocb_prepare_L_pointers_array_blk64 (c, Ls, blkn);

	  /* Process data in 64 block chunks. */
	  while (nblocks >= 64)
	    {
	      blkn += 64;
	      *l = (uintptr_t)(void *)ocb_get_l(c, blkn - blkn % 64);

	      bulk_ocb_fn (ctx, outbuf, inbuf, c->u_iv.iv, c->u_ctr.ctr, Ls);

	      nblocks -= 64;
	      outbuf += 64 * CAMELLIA_BLOCK_SIZE;
	      inbuf  += 64 * CAMELLIA_BLOCK_SIZE;
	      did_use_gfni_avx512 = 1;
	    }
	}

      if (did_use_gfni_avx512)
	{
	  if (burn_stack_depth < avx2_burn_stack_depth)
	    burn_stack_depth = avx2_burn_stack_depth;
	}

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX2
  if (ctx->use_avx2)
    {
      int did_use_aesni_avx2 = 0;
      u64 Ls[32];
      u64 *l;

      if (nblocks >= 32)
	{
	  typeof (&_gcry_camellia_aesni_avx2_ocb_dec) bulk_ocb_fn =
	      encrypt ? _gcry_camellia_aesni_avx2_ocb_enc
		      : _gcry_camellia_aesni_avx2_ocb_dec;

#ifdef USE_VAES_AVX2
	  if (ctx->use_vaes_avx2)
	    bulk_ocb_fn = encrypt ? _gcry_camellia_vaes_avx2_ocb_enc
				  : _gcry_camellia_vaes_avx2_ocb_dec;
#endif
#ifdef USE_GFNI_AVX2
	  if (ctx->use_gfni_avx2)
	    bulk_ocb_fn = encrypt ? _gcry_camellia_gfni_avx2_ocb_enc
				  : _gcry_camellia_gfni_avx2_ocb_dec;
#endif
          l = bulk_ocb_prepare_L_pointers_array_blk32 (c, Ls, blkn);

	  /* Process data in 32 block chunks. */
	  while (nblocks >= 32)
	    {
	      blkn += 32;
	      *l = (uintptr_t)(void *)ocb_get_l(c, blkn - blkn % 32);

	      bulk_ocb_fn (ctx, outbuf, inbuf, c->u_iv.iv, c->u_ctr.ctr, Ls);

	      nblocks -= 32;
	      outbuf += 32 * CAMELLIA_BLOCK_SIZE;
	      inbuf  += 32 * CAMELLIA_BLOCK_SIZE;
	      did_use_aesni_avx2 = 1;
	    }
	}

      if (did_use_aesni_avx2)
	{
	  if (burn_stack_depth < avx2_burn_stack_depth)
	    burn_stack_depth = avx2_burn_stack_depth;
	}

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX
  if (ctx->use_aesni_avx)
    {
      int did_use_aesni_avx = 0;
      u64 Ls[16];
      u64 *l;

      if (nblocks >= 16)
	{
          l = bulk_ocb_prepare_L_pointers_array_blk16 (c, Ls, blkn);

	  /* Process data in 16 block chunks. */
	  while (nblocks >= 16)
	    {
	      blkn += 16;
	      *l = (uintptr_t)(void *)ocb_get_l(c, blkn - blkn % 16);

	      if (encrypt)
		_gcry_camellia_aesni_avx_ocb_enc(ctx, outbuf, inbuf, c->u_iv.iv,
						c->u_ctr.ctr, Ls);
	      else
		_gcry_camellia_aesni_avx_ocb_dec(ctx, outbuf, inbuf, c->u_iv.iv,
						c->u_ctr.ctr, Ls);

	      nblocks -= 16;
	      outbuf += 16 * CAMELLIA_BLOCK_SIZE;
	      inbuf  += 16 * CAMELLIA_BLOCK_SIZE;
	      did_use_aesni_avx = 1;
	    }
	}

      if (did_use_aesni_avx)
	{
	  if (burn_stack_depth < avx_burn_stack_depth)
	    burn_stack_depth = avx_burn_stack_depth;
	}

      /* Use generic code to handle smaller chunks... */
    }
#endif

#if defined(USE_PPC_CRYPTO) || defined(USE_AESNI_AVX) || defined(USE_AESNI_AVX2)
  /* Process remaining blocks. */
  if (nblocks)
    {
      byte tmpbuf[CAMELLIA_BLOCK_SIZE * 32];
      unsigned int tmp_used = CAMELLIA_BLOCK_SIZE;
      size_t nburn;

      nburn = bulk_ocb_crypt_128 (c, ctx, encrypt ? camellia_encrypt_blk1_32
                                                  : camellia_decrypt_blk1_32,
                                  outbuf, inbuf, nblocks, &blkn, encrypt,
                                  tmpbuf, sizeof(tmpbuf) / CAMELLIA_BLOCK_SIZE,
                                  &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory(tmpbuf, tmp_used);
      nblocks = 0;
    }

  c->u_mode.ocb.data_nblocks = blkn;

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth + 4 * sizeof(void *));
#endif

  return nblocks;
}

/* Bulk authentication of complete blocks in OCB mode. */
static size_t
_gcry_camellia_ocb_auth (gcry_cipher_hd_t c, const void *abuf_arg,
			 size_t nblocks)
{
#if defined(USE_PPC_CRYPTO) || defined(USE_AESNI_AVX) || defined(USE_AESNI_AVX2)
  CAMELLIA_context *ctx = (void *)&c->context.c;
  const unsigned char *abuf = abuf_arg;
  int burn_stack_depth = 0;
  u64 blkn = c->u_mode.ocb.aad_nblocks;
#else
  (void)c;
  (void)abuf_arg;
#endif

#ifdef USE_AESNI_AVX2
  if (ctx->use_avx2)
    {
      int did_use_aesni_avx2 = 0;
      u64 Ls[32];
      u64 *l;

      if (nblocks >= 32)
	{
	  typeof (&_gcry_camellia_aesni_avx2_ocb_auth) bulk_auth_fn =
	      _gcry_camellia_aesni_avx2_ocb_auth;

#ifdef USE_VAES_AVX2
	  if (ctx->use_vaes_avx2)
	    bulk_auth_fn = _gcry_camellia_vaes_avx2_ocb_auth;
#endif
#ifdef USE_GFNI_AVX2
	  if (ctx->use_gfni_avx2)
	    bulk_auth_fn = _gcry_camellia_gfni_avx2_ocb_auth;
#endif

          l = bulk_ocb_prepare_L_pointers_array_blk32 (c, Ls, blkn);

	  /* Process data in 32 block chunks. */
	  while (nblocks >= 32)
	    {
	      blkn += 32;
	      *l = (uintptr_t)(void *)ocb_get_l(c, blkn - blkn % 32);

	      bulk_auth_fn (ctx, abuf, c->u_mode.ocb.aad_offset,
			    c->u_mode.ocb.aad_sum, Ls);

	      nblocks -= 32;
	      abuf += 32 * CAMELLIA_BLOCK_SIZE;
	      did_use_aesni_avx2 = 1;
	    }
	}

      if (did_use_aesni_avx2)
	{
	  if (burn_stack_depth < avx2_burn_stack_depth)
	    burn_stack_depth = avx2_burn_stack_depth;
	}

      /* Use generic code to handle smaller chunks... */
    }
#endif

#ifdef USE_AESNI_AVX
  if (ctx->use_aesni_avx)
    {
      int did_use_aesni_avx = 0;
      u64 Ls[16];
      u64 *l;

      if (nblocks >= 16)
	{
          l = bulk_ocb_prepare_L_pointers_array_blk16 (c, Ls, blkn);

	  /* Process data in 16 block chunks. */
	  while (nblocks >= 16)
	    {
	      blkn += 16;
	      *l = (uintptr_t)(void *)ocb_get_l(c, blkn - blkn % 16);

	      _gcry_camellia_aesni_avx_ocb_auth(ctx, abuf,
						c->u_mode.ocb.aad_offset,
						c->u_mode.ocb.aad_sum, Ls);

	      nblocks -= 16;
	      abuf += 16 * CAMELLIA_BLOCK_SIZE;
	      did_use_aesni_avx = 1;
	    }
	}

      if (did_use_aesni_avx)
	{
	  if (burn_stack_depth < avx_burn_stack_depth)
	    burn_stack_depth = avx_burn_stack_depth;
	}

      /* Use generic code to handle smaller chunks... */
    }
#endif

#if defined(USE_PPC_CRYPTO) || defined(USE_AESNI_AVX) || defined(USE_AESNI_AVX2)
  /* Process remaining blocks. */
  if (nblocks)
    {
      byte tmpbuf[CAMELLIA_BLOCK_SIZE * 32];
      unsigned int tmp_used = CAMELLIA_BLOCK_SIZE;
      size_t nburn;

      nburn = bulk_ocb_auth_128 (c, ctx, camellia_encrypt_blk1_32,
                                 abuf, nblocks, &blkn, tmpbuf,
                                 sizeof(tmpbuf) / CAMELLIA_BLOCK_SIZE,
                                 &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory(tmpbuf, tmp_used);
      nblocks = 0;
    }

  c->u_mode.ocb.aad_nblocks = blkn;

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth + 4 * sizeof(void *));
#endif

  return nblocks;
}


static const char *
selftest(void)
{
  CAMELLIA_context ctx;
  byte scratch[16];
  cipher_bulk_ops_t bulk_ops;

  /* These test vectors are from RFC-3713 */
  static const byte plaintext[]=
    {
      0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
      0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
  static const byte key_128[]=
    {
      0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
      0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
  static const byte ciphertext_128[]=
    {
      0x67,0x67,0x31,0x38,0x54,0x96,0x69,0x73,
      0x08,0x57,0x06,0x56,0x48,0xea,0xbe,0x43
    };
  static const byte key_192[]=
    {
      0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xfe,0xdc,0xba,0x98,
      0x76,0x54,0x32,0x10,0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77
    };
  static const byte ciphertext_192[]=
    {
      0xb4,0x99,0x34,0x01,0xb3,0xe9,0x96,0xf8,
      0x4e,0xe5,0xce,0xe7,0xd7,0x9b,0x09,0xb9
    };
  static const byte key_256[]=
    {
      0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xfe,0xdc,0xba,
      0x98,0x76,0x54,0x32,0x10,0x00,0x11,0x22,0x33,0x44,0x55,
      0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
    };
  static const byte ciphertext_256[]=
    {
      0x9a,0xcc,0x23,0x7d,0xff,0x16,0xd7,0x6c,
      0x20,0xef,0x7c,0x91,0x9e,0x3a,0x75,0x09
    };

  camellia_setkey(&ctx,key_128,sizeof(key_128),&bulk_ops);
  camellia_encrypt(&ctx,scratch,plaintext);
  if(memcmp(scratch,ciphertext_128,sizeof(ciphertext_128))!=0)
    return "CAMELLIA-128 test encryption failed.";
  camellia_decrypt(&ctx,scratch,scratch);
  if(memcmp(scratch,plaintext,sizeof(plaintext))!=0)
    return "CAMELLIA-128 test decryption failed.";

  camellia_setkey(&ctx,key_192,sizeof(key_192),&bulk_ops);
  camellia_encrypt(&ctx,scratch,plaintext);
  if(memcmp(scratch,ciphertext_192,sizeof(ciphertext_192))!=0)
    return "CAMELLIA-192 test encryption failed.";
  camellia_decrypt(&ctx,scratch,scratch);
  if(memcmp(scratch,plaintext,sizeof(plaintext))!=0)
    return "CAMELLIA-192 test decryption failed.";

  camellia_setkey(&ctx,key_256,sizeof(key_256),&bulk_ops);
  camellia_encrypt(&ctx,scratch,plaintext);
  if(memcmp(scratch,ciphertext_256,sizeof(ciphertext_256))!=0)
    return "CAMELLIA-256 test encryption failed.";
  camellia_decrypt(&ctx,scratch,scratch);
  if(memcmp(scratch,plaintext,sizeof(plaintext))!=0)
    return "CAMELLIA-256 test decryption failed.";

  return NULL;
}

/* These oids are from
   <http://info.isl.ntt.co.jp/crypt/eng/camellia/specifications_oid.html>,
   retrieved May 1, 2007. */

static const gcry_cipher_oid_spec_t camellia128_oids[] =
  {
    {"1.2.392.200011.61.1.1.1.2", GCRY_CIPHER_MODE_CBC},
    {"0.3.4401.5.3.1.9.1", GCRY_CIPHER_MODE_ECB},
    {"0.3.4401.5.3.1.9.3", GCRY_CIPHER_MODE_OFB},
    {"0.3.4401.5.3.1.9.4", GCRY_CIPHER_MODE_CFB},
    { NULL }
  };

static const gcry_cipher_oid_spec_t camellia192_oids[] =
  {
    {"1.2.392.200011.61.1.1.1.3", GCRY_CIPHER_MODE_CBC},
    {"0.3.4401.5.3.1.9.21", GCRY_CIPHER_MODE_ECB},
    {"0.3.4401.5.3.1.9.23", GCRY_CIPHER_MODE_OFB},
    {"0.3.4401.5.3.1.9.24", GCRY_CIPHER_MODE_CFB},
    { NULL }
  };

static const gcry_cipher_oid_spec_t camellia256_oids[] =
  {
    {"1.2.392.200011.61.1.1.1.4", GCRY_CIPHER_MODE_CBC},
    {"0.3.4401.5.3.1.9.41", GCRY_CIPHER_MODE_ECB},
    {"0.3.4401.5.3.1.9.43", GCRY_CIPHER_MODE_OFB},
    {"0.3.4401.5.3.1.9.44", GCRY_CIPHER_MODE_CFB},
    { NULL }
  };

gcry_cipher_spec_t _gcry_cipher_spec_camellia128 =
  {
    GCRY_CIPHER_CAMELLIA128, {0, 0},
    "CAMELLIA128",NULL,camellia128_oids,CAMELLIA_BLOCK_SIZE,128,
    sizeof(CAMELLIA_context),camellia_setkey,camellia_encrypt,camellia_decrypt
  };

gcry_cipher_spec_t _gcry_cipher_spec_camellia192 =
  {
    GCRY_CIPHER_CAMELLIA192, {0, 0},
    "CAMELLIA192",NULL,camellia192_oids,CAMELLIA_BLOCK_SIZE,192,
    sizeof(CAMELLIA_context),camellia_setkey,camellia_encrypt,camellia_decrypt
  };

gcry_cipher_spec_t _gcry_cipher_spec_camellia256 =
  {
    GCRY_CIPHER_CAMELLIA256, {0, 0},
    "CAMELLIA256",NULL,camellia256_oids,CAMELLIA_BLOCK_SIZE,256,
    sizeof(CAMELLIA_context),camellia_setkey,camellia_encrypt,camellia_decrypt
  };
