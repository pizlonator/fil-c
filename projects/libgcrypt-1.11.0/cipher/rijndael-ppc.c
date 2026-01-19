/* Rijndael (AES) for GnuPG - PowerPC Vector Crypto AES implementation
 * Copyright (C) 2019 Shawn Landden <shawn@git.icu>
 * Copyright (C) 2019-2020, 2022 Jussi Kivilinna <jussi.kivilinna@iki.fi>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, this code may be used in OpenSSL from The OpenSSL Project,
 * and Cryptogams by Andy Polyakov, and if made part of a release of either
 * or both projects, is thereafter dual-licensed under the license said project
 * is released under.
 */

#include <config.h>

#include "rijndael-internal.h"
#include "cipher-internal.h"
#include "bufhelp.h"

#ifdef USE_PPC_CRYPTO

#include "rijndael-ppc-common.h"


#ifdef HAVE_GCC_ATTRIBUTE_OPTIMIZE
# define FUNC_ATTR_OPT __attribute__((optimize("-O2")))
#else
# define FUNC_ATTR_OPT
#endif

#if defined(__clang__) && defined(HAVE_CLANG_ATTRIBUTE_PPC_TARGET)
# define PPC_OPT_ATTR __attribute__((target("arch=pwr8"))) FUNC_ATTR_OPT
#elif defined(HAVE_GCC_ATTRIBUTE_PPC_TARGET)
# define PPC_OPT_ATTR __attribute__((target("cpu=power8"))) FUNC_ATTR_OPT
#else
# define PPC_OPT_ATTR FUNC_ATTR_OPT
#endif


#ifndef WORDS_BIGENDIAN
static const block vec_bswap32_const_neg =
  { ~3, ~2, ~1, ~0, ~7, ~6, ~5, ~4, ~11, ~10, ~9, ~8, ~15, ~14, ~13, ~12 };
#endif


static ASM_FUNC_ATTR_INLINE block
asm_load_be_const(void)
{
#ifndef WORDS_BIGENDIAN
  return ALIGNED_LOAD (&vec_bswap32_const_neg, 0);
#else
  static const block vec_dummy = { 0 };
  return vec_dummy;
#endif
}

static ASM_FUNC_ATTR_INLINE block
asm_be_swap(block vec, block be_bswap_const)
{
  (void)be_bswap_const;
#ifndef WORDS_BIGENDIAN
  return asm_vperm1 (vec, be_bswap_const);
#else
  return vec;
#endif
}

static ASM_FUNC_ATTR_INLINE block
asm_load_be_noswap(unsigned long offset, const void *ptr)
{
  block vec;
#if __GNUC__ >= 4
  if (__builtin_constant_p (offset) && offset == 0)
    __asm__ volatile ("lxvw4x %x0,0,%1\n\t"
		      : "=wa" (vec)
		      : "r" ((uintptr_t)ptr)
		      : "memory");
  else
#endif
    __asm__ volatile ("lxvw4x %x0,%1,%2\n\t"
		      : "=wa" (vec)
		      : "r" (offset), "r" ((uintptr_t)ptr)
		      : "memory", "r0");
  /* NOTE: vec needs to be be-swapped using 'asm_be_swap' by caller */
  return vec;
}

static ASM_FUNC_ATTR_INLINE void
asm_store_be_noswap(block vec, unsigned long offset, void *ptr)
{
  /* NOTE: vec be-swapped using 'asm_be_swap' by caller */
#if __GNUC__ >= 4
  if (__builtin_constant_p (offset) && offset == 0)
    __asm__ volatile ("stxvw4x %x0,0,%1\n\t"
		      :
		      : "wa" (vec), "r" ((uintptr_t)ptr)
		      : "memory");
  else
#endif
    __asm__ volatile ("stxvw4x %x0,%1,%2\n\t"
		      :
		      : "wa" (vec), "r" (offset), "r" ((uintptr_t)ptr)
		      : "memory", "r0");
}


static ASM_FUNC_ATTR_INLINE unsigned int
keysched_idx(unsigned int in)
{
#ifdef WORDS_BIGENDIAN
  return in;
#else
  return (in & ~3U) | (3U - (in & 3U));
#endif
}


static ASM_FUNC_ATTR_INLINE vec_u32
bcast_u32_to_vec(u32 x)
{
  vec_u32 v = { x, x, x, x };
  return v;
}


static ASM_FUNC_ATTR_INLINE u32
u32_from_vec(vec_u32 x)
{
#ifdef WORDS_BIGENDIAN
  return x[1];
#else
  return x[2];
#endif
}


void PPC_OPT_ATTR
_gcry_aes_ppc8_setkey (RIJNDAEL_context *ctx, const byte *key)
{
  static const vec_u32 rotate24 = { 24, 24, 24, 24 };
  static const vec_u32 rcon_const = { 0x1b, 0x1b, 0x1b, 0x1b };
  vec_u32 tk_vu32[MAXKC];
  unsigned int rounds = ctx->rounds;
  unsigned int KC = rounds - 6;
  u32 *W_u32 = ctx->keyschenc32b;
  unsigned int i, j;
  vec_u32 tk_prev;
  vec_u32 rcon = { 1, 1, 1, 1 };

  for (i = 0; i < KC; i += 2)
    {
      unsigned int idx0 = keysched_idx(i + 0);
      unsigned int idx1 = keysched_idx(i + 1);
      tk_vu32[i + 0] = bcast_u32_to_vec(buf_get_le32(key + i * 4 + 0));
      tk_vu32[i + 1] = bcast_u32_to_vec(buf_get_le32(key + i * 4 + 4));
      W_u32[idx0] = u32_from_vec(vec_revb(tk_vu32[i + 0]));
      W_u32[idx1] = u32_from_vec(vec_revb(tk_vu32[i + 1]));
    }

  for (i = KC, j = KC, tk_prev = tk_vu32[KC - 1];
       i < 4 * (rounds + 1);
       i += 2, j += 2)
    {
      unsigned int idx0 = keysched_idx(i + 0);
      unsigned int idx1 = keysched_idx(i + 1);
      vec_u32 temp0 = tk_prev;
      vec_u32 temp1;

      if (j == KC)
        {
          j = 0;
          temp0 = (vec_u32)(asm_sbox_be((block)vec_rl(temp0, rotate24))) ^ rcon;
          rcon = (vec_u32)(((block)rcon << 1)
                           ^ (-((block)rcon >> 7) & (block)rcon_const));
        }
      else if (KC == 8 && j == 4)
        {
          temp0 = (vec_u32)asm_sbox_be((block)temp0);
        }

      temp1 = tk_vu32[j + 0];

      tk_vu32[j + 0] = temp0 ^ temp1;
      tk_vu32[j + 1] ^= temp0 ^ temp1;
      tk_prev = tk_vu32[j + 1];

      W_u32[idx0] = u32_from_vec(vec_revb(tk_vu32[j + 0]));
      W_u32[idx1] = u32_from_vec(vec_revb(tk_vu32[j + 1]));
    }

  wipememory(tk_vu32, sizeof(tk_vu32));
}


void PPC_OPT_ATTR
_gcry_aes_ppc8_prepare_decryption (RIJNDAEL_context *ctx)
{
  internal_aes_ppc_prepare_decryption (ctx);
}


#define GCRY_AES_PPC8 1
#define ENCRYPT_BLOCK_FUNC	_gcry_aes_ppc8_encrypt
#define DECRYPT_BLOCK_FUNC	_gcry_aes_ppc8_decrypt
#define ECB_CRYPT_FUNC		_gcry_aes_ppc8_ecb_crypt
#define CFB_ENC_FUNC		_gcry_aes_ppc8_cfb_enc
#define CFB_DEC_FUNC		_gcry_aes_ppc8_cfb_dec
#define CBC_ENC_FUNC		_gcry_aes_ppc8_cbc_enc
#define CBC_DEC_FUNC		_gcry_aes_ppc8_cbc_dec
#define CTR_ENC_FUNC		_gcry_aes_ppc8_ctr_enc
#define OCB_CRYPT_FUNC		_gcry_aes_ppc8_ocb_crypt
#define OCB_AUTH_FUNC		_gcry_aes_ppc8_ocb_auth
#define XTS_CRYPT_FUNC		_gcry_aes_ppc8_xts_crypt
#define CTR32LE_ENC_FUNC	_gcry_aes_ppc8_ctr32le_enc

#include <rijndael-ppc-functions.h>

#endif /* USE_PPC_CRYPTO */
