/* bulkhelp.h  -  Some bulk processing helpers
 * Copyright (C) 2022 Jussi Kivilinna <jussi.kivilinna@iki.fi>
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
 */
#ifndef GCRYPT_BULKHELP_H
#define GCRYPT_BULKHELP_H


#include "g10lib.h"
#include "cipher-internal.h"


#ifdef __x86_64__
/* Use u64 to store pointers for x32 support (assembly function assumes
 * 64-bit pointers). */
typedef u64 ocb_L_uintptr_t;
#else
typedef uintptr_t ocb_L_uintptr_t;
#endif

typedef unsigned int (*bulk_crypt_fn_t) (void *ctx, byte *out,
                                         const byte *in,
                                         size_t num_blks);


static inline ocb_L_uintptr_t *
bulk_ocb_prepare_L_pointers_array_blk64 (gcry_cipher_hd_t c,
                                         ocb_L_uintptr_t Ls[64], u64 blkn)
{
  unsigned int n = 64 - (blkn % 64);
  unsigned int i;

  for (i = 0; i < 64; i += 8)
    {
      Ls[(i + 0 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 1 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[1];
      Ls[(i + 2 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 3 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[2];
      Ls[(i + 4 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 5 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[1];
      Ls[(i + 6 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
    }

  Ls[(7 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[3];
  Ls[(15 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[4];
  Ls[(23 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[3];
  Ls[(31 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[5];
  Ls[(39 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[3];
  Ls[(47 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[4];
  Ls[(55 + n) % 64] = (uintptr_t)(void *)c->u_mode.ocb.L[3];
  return &Ls[(63 + n) % 64];
}


static inline ocb_L_uintptr_t *
bulk_ocb_prepare_L_pointers_array_blk32 (gcry_cipher_hd_t c,
                                         ocb_L_uintptr_t Ls[32], u64 blkn)
{
  unsigned int n = 32 - (blkn % 32);
  unsigned int i;

  for (i = 0; i < 32; i += 8)
    {
      Ls[(i + 0 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 1 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[1];
      Ls[(i + 2 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 3 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[2];
      Ls[(i + 4 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 5 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[1];
      Ls[(i + 6 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
    }

  Ls[(7 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[3];
  Ls[(15 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[4];
  Ls[(23 + n) % 32] = (uintptr_t)(void *)c->u_mode.ocb.L[3];
  return &Ls[(31 + n) % 32];
}


static inline ocb_L_uintptr_t *
bulk_ocb_prepare_L_pointers_array_blk16 (gcry_cipher_hd_t c,
                                         ocb_L_uintptr_t Ls[16], u64 blkn)
{
  unsigned int n = 16 - (blkn % 16);
  unsigned int i;

  for (i = 0; i < 16; i += 8)
    {
      Ls[(i + 0 + n) % 16] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 1 + n) % 16] = (uintptr_t)(void *)c->u_mode.ocb.L[1];
      Ls[(i + 2 + n) % 16] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 3 + n) % 16] = (uintptr_t)(void *)c->u_mode.ocb.L[2];
      Ls[(i + 4 + n) % 16] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
      Ls[(i + 5 + n) % 16] = (uintptr_t)(void *)c->u_mode.ocb.L[1];
      Ls[(i + 6 + n) % 16] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
    }

  Ls[(7 + n) % 16] = (uintptr_t)(void *)c->u_mode.ocb.L[3];
  return &Ls[(15 + n) % 16];
}


static inline ocb_L_uintptr_t *
bulk_ocb_prepare_L_pointers_array_blk8 (gcry_cipher_hd_t c,
                                        ocb_L_uintptr_t Ls[8], u64 blkn)
{
  unsigned int n = 8 - (blkn % 8);

  Ls[(0 + n) % 8] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
  Ls[(1 + n) % 8] = (uintptr_t)(void *)c->u_mode.ocb.L[1];
  Ls[(2 + n) % 8] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
  Ls[(3 + n) % 8] = (uintptr_t)(void *)c->u_mode.ocb.L[2];
  Ls[(4 + n) % 8] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
  Ls[(5 + n) % 8] = (uintptr_t)(void *)c->u_mode.ocb.L[1];
  Ls[(6 + n) % 8] = (uintptr_t)(void *)c->u_mode.ocb.L[0];
  Ls[(7 + n) % 8] = (uintptr_t)(void *)c->u_mode.ocb.L[3];

  return &Ls[(7 + n) % 8];
}


static inline unsigned int
bulk_ctr_enc_128 (void *priv, bulk_crypt_fn_t crypt_fn, byte *outbuf,
                  const byte *inbuf, size_t nblocks, byte *ctr,
                  byte *tmpbuf, size_t tmpbuf_nblocks,
                  unsigned int *num_used_tmpblocks)
{
  unsigned int tmp_used = 16;
  unsigned int burn_depth = 0;
  unsigned int nburn;

  while (nblocks >= 1)
    {
      size_t curr_blks = nblocks > tmpbuf_nblocks ? tmpbuf_nblocks : nblocks;
      size_t i;

      if (curr_blks * 16 > tmp_used)
        tmp_used = curr_blks * 16;

      cipher_block_cpy (tmpbuf + 0 * 16, ctr, 16);
      for (i = 1; i < curr_blks; i++)
        {
          cipher_block_cpy (&tmpbuf[i * 16], ctr, 16);
          cipher_block_add (&tmpbuf[i * 16], i, 16);
        }
      cipher_block_add (ctr, curr_blks, 16);

      nburn = crypt_fn (priv, tmpbuf, tmpbuf, curr_blks);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      for (i = 0; i < curr_blks; i++)
        {
          cipher_block_xor (outbuf, &tmpbuf[i * 16], inbuf, 16);
          outbuf += 16;
          inbuf += 16;
        }

      nblocks -= curr_blks;
    }

  *num_used_tmpblocks = tmp_used;
  return burn_depth;
}


static inline unsigned int
bulk_ctr32le_enc_128 (void *priv, bulk_crypt_fn_t crypt_fn, byte *outbuf,
                      const byte *inbuf, size_t nblocks, byte *ctr,
                      byte *tmpbuf, size_t tmpbuf_nblocks,
                      unsigned int *num_used_tmpblocks)
{
  unsigned int tmp_used = 16;
  unsigned int burn_depth = 0;
  unsigned int nburn;

  while (nblocks >= 1)
    {
      size_t curr_blks = nblocks > tmpbuf_nblocks ? tmpbuf_nblocks : nblocks;
      u64 ctr_lo = buf_get_le64(ctr + 0 * 8);
      u64 ctr_hi = buf_get_he64(ctr + 1 * 8);
      size_t i;

      if (curr_blks * 16 > tmp_used)
        tmp_used = curr_blks * 16;

      cipher_block_cpy (tmpbuf + 0 * 16, ctr, 16);
      for (i = 1; i < curr_blks; i++)
        {
          u32 lo_u32 = (u32)ctr_lo + i;
          u64 lo_u64 = ctr_lo & ~(u64)(u32)-1;
          lo_u64 += lo_u32;
          buf_put_le64(&tmpbuf[0 * 8 + i * 16], lo_u64);
          buf_put_he64(&tmpbuf[1 * 8 + i * 16], ctr_hi);
        }
      buf_put_le32(ctr, (u32)ctr_lo + curr_blks);

      nburn = crypt_fn (priv, tmpbuf, tmpbuf, curr_blks);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      for (i = 0; i < curr_blks; i++)
        {
          cipher_block_xor (outbuf, &tmpbuf[i * 16], inbuf, 16);
          outbuf += 16;
          inbuf += 16;
        }

      nblocks -= curr_blks;
    }

  *num_used_tmpblocks = tmp_used;
  return burn_depth;
}


static inline unsigned int
bulk_cbc_dec_128 (void *priv, bulk_crypt_fn_t crypt_fn, byte *outbuf,
                  const byte *inbuf, size_t nblocks, byte *iv,
                  byte *tmpbuf, size_t tmpbuf_nblocks,
                  unsigned int *num_used_tmpblocks)
{
  unsigned int tmp_used = 16;
  unsigned int burn_depth = 0;
  unsigned int nburn;

  while (nblocks >= 1)
    {
      size_t curr_blks = nblocks > tmpbuf_nblocks ? tmpbuf_nblocks : nblocks;
      size_t i;

      if (curr_blks * 16 > tmp_used)
        tmp_used = curr_blks * 16;

      nburn = crypt_fn (priv, tmpbuf, inbuf, curr_blks);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      for (i = 0; i < curr_blks; i++)
        {
          cipher_block_xor_n_copy_2(outbuf, &tmpbuf[i * 16], iv, inbuf, 16);
          outbuf += 16;
          inbuf += 16;
        }

      nblocks -= curr_blks;
    }

  *num_used_tmpblocks = tmp_used;
  return burn_depth;
}


static inline unsigned int
bulk_cfb_dec_128 (void *priv, bulk_crypt_fn_t crypt_fn, byte *outbuf,
                  const byte *inbuf, size_t nblocks, byte *iv,
                  byte *tmpbuf, size_t tmpbuf_nblocks,
                  unsigned int *num_used_tmpblocks)
{
  unsigned int tmp_used = 16;
  unsigned int burn_depth = 0;
  unsigned int nburn;

  while (nblocks >= 1)
    {
      size_t curr_blks = nblocks > tmpbuf_nblocks ? tmpbuf_nblocks : nblocks;
      size_t i;

      if (curr_blks * 16 > tmp_used)
        tmp_used = curr_blks * 16;

      cipher_block_cpy (&tmpbuf[0 * 16], iv, 16);
      if (curr_blks > 1)
        memcpy (&tmpbuf[1 * 16], &inbuf[(1 - 1) * 16], 16 * curr_blks - 16);
      cipher_block_cpy (iv, &inbuf[(curr_blks - 1) * 16], 16);

      nburn = crypt_fn (priv, tmpbuf, tmpbuf, curr_blks);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      for (i = 0; i < curr_blks; i++)
        {
          cipher_block_xor (outbuf, inbuf, &tmpbuf[i * 16], 16);
          outbuf += 16;
          inbuf += 16;
        }

      nblocks -= curr_blks;
    }

  *num_used_tmpblocks = tmp_used;
  return burn_depth;
}


static inline unsigned int
bulk_ocb_crypt_128 (gcry_cipher_hd_t c, void *priv, bulk_crypt_fn_t crypt_fn,
                    byte *outbuf, const byte *inbuf, size_t nblocks, u64 *blkn,
                    int encrypt, byte *tmpbuf, size_t tmpbuf_nblocks,
                    unsigned int *num_used_tmpblocks)
{
  unsigned int tmp_used = 16;
  unsigned int burn_depth = 0;
  unsigned int nburn;

  while (nblocks >= 1)
    {
      size_t curr_blks = nblocks > tmpbuf_nblocks ? tmpbuf_nblocks : nblocks;
      size_t i;

      if (curr_blks * 16 > tmp_used)
        tmp_used = curr_blks * 16;

      for (i = 0; i < curr_blks; i++)
        {
          const unsigned char *l = ocb_get_l(c, ++*blkn);

          /* Checksum_i = Checksum_{i-1} xor P_i  */
          if (encrypt)
            cipher_block_xor_1(c->u_ctr.ctr, &inbuf[i * 16], 16);

          /* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
          cipher_block_xor_2dst (&tmpbuf[i * 16], c->u_iv.iv, l, 16);
          cipher_block_xor (&outbuf[i * 16], &inbuf[i * 16],
                            c->u_iv.iv, 16);
        }

      /* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */
      nburn = crypt_fn (priv, outbuf, outbuf, curr_blks);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      for (i = 0; i < curr_blks; i++)
        {
          cipher_block_xor_1 (&outbuf[i * 16], &tmpbuf[i * 16], 16);

          /* Checksum_i = Checksum_{i-1} xor P_i  */
          if (!encrypt)
              cipher_block_xor_1(c->u_ctr.ctr, &outbuf[i * 16], 16);
        }

      outbuf += curr_blks * 16;
      inbuf  += curr_blks * 16;
      nblocks -= curr_blks;
    }

  *num_used_tmpblocks = tmp_used;
  return burn_depth;
}


static inline unsigned int
bulk_ocb_auth_128 (gcry_cipher_hd_t c, void *priv, bulk_crypt_fn_t crypt_fn,
                   const byte *abuf, size_t nblocks, u64 *blkn, byte *tmpbuf,
                   size_t tmpbuf_nblocks, unsigned int *num_used_tmpblocks)
{
  unsigned int tmp_used = 16;
  unsigned int burn_depth = 0;
  unsigned int nburn;

  while (nblocks >= 1)
    {
      size_t curr_blks = nblocks > tmpbuf_nblocks ? tmpbuf_nblocks : nblocks;
      size_t i;

      if (curr_blks * 16 > tmp_used)
        tmp_used = curr_blks * 16;

      for (i = 0; i < curr_blks; i++)
        {
          const unsigned char *l = ocb_get_l(c, ++*blkn);

          /* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
          cipher_block_xor_2dst (&tmpbuf[i * 16],
                                  c->u_mode.ocb.aad_offset, l, 16);
          cipher_block_xor_1 (&tmpbuf[i * 16], &abuf[i * 16], 16);
        }

      /* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */
      nburn = crypt_fn (priv, tmpbuf, tmpbuf, curr_blks);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      for (i = 0; i < curr_blks; i++)
        {
          cipher_block_xor_1 (c->u_mode.ocb.aad_sum, &tmpbuf[i * 16], 16);
        }

      abuf += curr_blks * 16;
      nblocks -= curr_blks;
    }

  *num_used_tmpblocks = tmp_used;
  return burn_depth;
}


static inline unsigned int
bulk_xts_crypt_128 (void *priv, bulk_crypt_fn_t crypt_fn, byte *outbuf,
                    const byte *inbuf, size_t nblocks, byte *tweak,
                    byte *tmpbuf, size_t tmpbuf_nblocks,
                    unsigned int *num_used_tmpblocks)
{
  u64 tweak_lo, tweak_hi, tweak_next_lo, tweak_next_hi, tmp_lo, tmp_hi, carry;
  unsigned int tmp_used = 16;
  unsigned int burn_depth = 0;
  unsigned int nburn;

  tweak_next_lo = buf_get_le64 (tweak + 0);
  tweak_next_hi = buf_get_le64 (tweak + 8);

  while (nblocks >= 1)
    {
      size_t curr_blks = nblocks > tmpbuf_nblocks ? tmpbuf_nblocks : nblocks;
      size_t i;

      if (curr_blks * 16 > tmp_used)
        tmp_used = curr_blks * 16;

      for (i = 0; i < curr_blks; i++)
        {
          tweak_lo = tweak_next_lo;
          tweak_hi = tweak_next_hi;

          /* Generate next tweak. */
          carry = -(tweak_next_hi >> 63) & 0x87;
          tweak_next_hi = (tweak_next_hi << 1) + (tweak_next_lo >> 63);
          tweak_next_lo = (tweak_next_lo << 1) ^ carry;

          /* Xor-Encrypt/Decrypt-Xor block. */
          tmp_lo = buf_get_le64 (inbuf + i * 16 + 0) ^ tweak_lo;
          tmp_hi = buf_get_le64 (inbuf + i * 16 + 8) ^ tweak_hi;
          buf_put_he64 (&tmpbuf[i * 16 + 0], tweak_lo);
          buf_put_he64 (&tmpbuf[i * 16 + 8], tweak_hi);
          buf_put_le64 (outbuf + i * 16 + 0, tmp_lo);
          buf_put_le64 (outbuf + i * 16 + 8, tmp_hi);
        }

      nburn = crypt_fn (priv, outbuf, outbuf, curr_blks);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      for (i = 0; i < curr_blks; i++)
        {
          /* Xor-Encrypt/Decrypt-Xor block. */
          tweak_lo = buf_get_he64 (&tmpbuf[i * 16 + 0]);
          tweak_hi = buf_get_he64 (&tmpbuf[i * 16 + 8]);
          tmp_lo = buf_get_le64 (outbuf + i * 16 + 0) ^ tweak_lo;
          tmp_hi = buf_get_le64 (outbuf + i * 16 + 8) ^ tweak_hi;
          buf_put_le64 (outbuf + i * 16 + 0, tmp_lo);
          buf_put_le64 (outbuf + i * 16 + 8, tmp_hi);
        }

      inbuf += curr_blks * 16;
      outbuf += curr_blks * 16;
      nblocks -= curr_blks;
    }

  buf_put_le64 (tweak + 0, tweak_next_lo);
  buf_put_le64 (tweak + 8, tweak_next_hi);

  *num_used_tmpblocks = tmp_used;
  return burn_depth;
}

static inline unsigned int
bulk_ecb_crypt_128 (void *priv, bulk_crypt_fn_t crypt_fn, byte *outbuf,
		    const byte *inbuf, size_t nblocks, size_t fn_max_nblocks)
{
  unsigned int burn_depth = 0;
  unsigned int nburn;

  while (nblocks >= 1)
    {
      size_t curr_blks = nblocks > fn_max_nblocks ? fn_max_nblocks : nblocks;
      nburn = crypt_fn (priv, outbuf, inbuf, curr_blks);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;
      inbuf += curr_blks * 16;
      outbuf += curr_blks * 16;
      nblocks -= curr_blks;
    }

  return burn_depth;
}

#endif /*GCRYPT_BULKHELP_H*/
