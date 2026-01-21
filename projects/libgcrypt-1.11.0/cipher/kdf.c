/* kdf.c  - Key Derivation Functions
 * Copyright (C) 1998, 2008, 2011 Free Software Foundation, Inc.
 * Copyright (C) 2013 g10 Code GmbH
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "g10lib.h"
#include "cipher.h"
#include "kdf-internal.h"


/* Transform a passphrase into a suitable key of length KEYSIZE and
   store this key in the caller provided buffer KEYBUFFER.  The caller
   must provide an HASHALGO, a valid ALGO and depending on that algo a
   SALT of 8 bytes and the number of ITERATIONS.  Code taken from
   gnupg/agent/protect.c:hash_passphrase.  */
static gpg_err_code_t
openpgp_s2k (const void *passphrase, size_t passphraselen,
             int algo, int hashalgo,
             const void *salt, size_t saltlen,
             unsigned long iterations,
             size_t keysize, void *keybuffer)
{
  gpg_err_code_t ec;
  gcry_md_hd_t md;
  char *key = keybuffer;
  int pass, i;
  int used = 0;
  int secmode;

  if ((algo == GCRY_KDF_SALTED_S2K || algo == GCRY_KDF_ITERSALTED_S2K)
      && (!salt || saltlen != 8))
    return GPG_ERR_INV_VALUE;

  secmode = _gcry_is_secure (passphrase) || _gcry_is_secure (keybuffer);

  ec = _gcry_md_open (&md, hashalgo, secmode? GCRY_MD_FLAG_SECURE : 0);
  if (ec)
    return ec;

  for (pass=0; used < keysize; pass++)
    {
      if (pass)
        {
          _gcry_md_reset (md);
          for (i=0; i < pass; i++) /* Preset the hash context.  */
            _gcry_md_putc (md, 0);
        }

      if (algo == GCRY_KDF_SALTED_S2K || algo == GCRY_KDF_ITERSALTED_S2K)
        {
          int len2 = passphraselen + 8;
          unsigned long count = len2;

          if (algo == GCRY_KDF_ITERSALTED_S2K)
            {
              count = iterations;
              if (count < len2)
                count = len2;
            }

          while (count > len2)
            {
              _gcry_md_write (md, salt, saltlen);
              _gcry_md_write (md, passphrase, passphraselen);
              count -= len2;
            }
          if (count < saltlen)
            _gcry_md_write (md, salt, count);
          else
            {
              _gcry_md_write (md, salt, saltlen);
              count -= saltlen;
              _gcry_md_write (md, passphrase, count);
            }
        }
      else
        _gcry_md_write (md, passphrase, passphraselen);

      _gcry_md_final (md);
      i = _gcry_md_get_algo_dlen (hashalgo);
      if (i > keysize - used)
        i = keysize - used;
      memcpy (key+used, _gcry_md_read (md, hashalgo), i);
      used += i;
    }
  _gcry_md_close (md);
  return 0;
}


/* Transform a passphrase into a suitable key of length KEYSIZE and
   store this key in the caller provided buffer KEYBUFFER.  The caller
   must provide PRFALGO which indicates the pseudorandom function to
   use: This shall be the algorithms id of a hash algorithm; it is
   used in HMAC mode.  SALT is a salt of length SALTLEN and ITERATIONS
   gives the number of iterations.  */
gpg_err_code_t
_gcry_kdf_pkdf2 (const void *passphrase, size_t passphraselen,
                 int hashalgo,
                 const void *salt, size_t saltlen,
                 unsigned long iterations,
                 size_t keysize, void *keybuffer)
{
  gpg_err_code_t ec;
  gcry_md_hd_t md;
  int secmode;
  unsigned long dklen = keysize;
  char *dk = keybuffer;
  unsigned int hlen;   /* Output length of the digest function.  */
  unsigned int l;      /* Rounded up number of blocks.  */
  unsigned int r;      /* Number of octets in the last block.  */
  char *sbuf;          /* Malloced buffer to concatenate salt and iter
                          as well as space to hold TBUF and UBUF.  */
  char *tbuf;          /* Buffer for T; ptr into SBUF, size is HLEN. */
  char *ubuf;          /* Buffer for U; ptr into SBUF, size is HLEN. */
  unsigned int lidx;   /* Current block number.  */
  unsigned long iter;  /* Current iteration number.  */
  unsigned int i;

  /* We allow for a saltlen of 0 here to support scrypt.  It is not
     clear whether rfc2898 allows for this this, thus we do a test on
     saltlen > 0 only in gcry_kdf_derive.  */
  if (!salt || !iterations || !dklen)
    return GPG_ERR_INV_VALUE;

  hlen = _gcry_md_get_algo_dlen (hashalgo);
  if (!hlen)
    return GPG_ERR_DIGEST_ALGO;

  secmode = _gcry_is_secure (passphrase) || _gcry_is_secure (keybuffer);

  /* Step 1 */
  /* If dkLen > (2^32 - 1) * hLen, output "derived key too long" and
   * stop.  We use a stronger inequality but only if our type can hold
   * a larger value.  */

#if SIZEOF_UNSIGNED_LONG > 4
  if (dklen > 0xffffffffU)
    return GPG_ERR_INV_VALUE;
#endif

  /* Step 2 */
  l = ((dklen - 1)/ hlen) + 1;
  r = dklen - (l - 1) * hlen;

  /* Setup buffers and prepare a hash context.  */
  sbuf = (secmode
          ? xtrymalloc_secure (saltlen + 4 + hlen + hlen)
          : xtrymalloc (saltlen + 4 + hlen + hlen));
  if (!sbuf)
    return gpg_err_code_from_syserror ();
  tbuf = sbuf + saltlen + 4;
  ubuf = tbuf + hlen;

  ec = _gcry_md_open (&md, hashalgo, (GCRY_MD_FLAG_HMAC
                                      | (secmode?GCRY_MD_FLAG_SECURE:0)));
  if (ec)
    {
      xfree (sbuf);
      return ec;
    }

  ec = _gcry_md_setkey (md, passphrase, passphraselen);
  if (ec)
    {
      _gcry_md_close (md);
      xfree (sbuf);
      return ec;
    }

  /* Step 3 and 4. */
  memcpy (sbuf, salt, saltlen);
  for (lidx = 1; lidx <= l; lidx++)
    {
      for (iter = 0; iter < iterations; iter++)
        {
          _gcry_md_reset (md);
          if (!iter) /* Compute U_1:  */
            {
              sbuf[saltlen]     = (lidx >> 24);
              sbuf[saltlen + 1] = (lidx >> 16);
              sbuf[saltlen + 2] = (lidx >> 8);
              sbuf[saltlen + 3] = lidx;
              _gcry_md_write (md, sbuf, saltlen + 4);
              memcpy (ubuf, _gcry_md_read (md, 0), hlen);
              memcpy (tbuf, ubuf, hlen);
            }
          else /* Compute U_(2..c):  */
            {
              _gcry_md_write (md, ubuf, hlen);
              memcpy (ubuf, _gcry_md_read (md, 0), hlen);
              for (i=0; i < hlen; i++)
                tbuf[i] ^= ubuf[i];
            }
        }
      if (lidx == l)  /* Last block.  */
        memcpy (dk, tbuf, r);
      else
        {
          memcpy (dk, tbuf, hlen);
          dk += hlen;
        }
    }

  _gcry_md_close (md);
  xfree (sbuf);
  return 0;
}


/* Derive a key from a passphrase.  KEYSIZE gives the requested size
   of the keys in octets.  KEYBUFFER is a caller provided buffer
   filled on success with the derived key.  The input passphrase is
   taken from (PASSPHRASE,PASSPHRASELEN) which is an arbitrary memory
   buffer.  ALGO specifies the KDF algorithm to use; these are the
   constants GCRY_KDF_*.  SUBALGO specifies an algorithm used
   internally by the KDF algorithms; this is usually a hash algorithm
   but certain KDF algorithm may use it differently.  {SALT,SALTLEN}
   is a salt as needed by most KDF algorithms.  ITERATIONS is a
   positive integer parameter to most KDFs.  0 is returned on success,
   or an error code on failure.  */
gpg_err_code_t
_gcry_kdf_derive (const void *passphrase, size_t passphraselen,
                  int algo, int subalgo,
                  const void *salt, size_t saltlen,
                  unsigned long iterations,
                  size_t keysize, void *keybuffer)
{
  gpg_err_code_t ec;

  if (!passphrase)
    {
      ec = GPG_ERR_INV_DATA;
      goto leave;
    }

  if (!keybuffer || !keysize)
    {
      ec = GPG_ERR_INV_VALUE;
      goto leave;
    }


  switch (algo)
    {
    case GCRY_KDF_SIMPLE_S2K:
    case GCRY_KDF_SALTED_S2K:
    case GCRY_KDF_ITERSALTED_S2K:
      if (!passphraselen)
        ec = GPG_ERR_INV_DATA;
      else
        ec = openpgp_s2k (passphrase, passphraselen, algo, subalgo,
                          salt, saltlen, iterations, keysize, keybuffer);
      break;

    case GCRY_KDF_PBKDF1:
      ec = GPG_ERR_UNSUPPORTED_ALGORITHM;
      break;

    case GCRY_KDF_PBKDF2:
      if (!saltlen)
        ec = GPG_ERR_INV_VALUE;
      else
        {
          /* FIPS requires minimum passphrase length, see FIPS 140-3 IG D.N */
          if (fips_mode () && passphraselen < 8)
            return GPG_ERR_INV_VALUE;

          /* FIPS requires minimum salt length of 128 b (SP 800-132 sec. 5.1, p.6) */
          if (fips_mode () && saltlen < 16)
            return GPG_ERR_INV_VALUE;

          /* FIPS requires minimum iterations bound (SP 800-132 sec 5.2, p.6) */
          if (fips_mode () && iterations < 1000)
            return GPG_ERR_INV_VALUE;

          /* Check minimum key size */
          if (fips_mode () && keysize < 14)
            return GPG_ERR_INV_VALUE;

          ec = _gcry_kdf_pkdf2 (passphrase, passphraselen, subalgo,
                                salt, saltlen, iterations, keysize, keybuffer);
        }
      break;

    case 41:
    case GCRY_KDF_SCRYPT:
#if USE_SCRYPT
      ec = _gcry_kdf_scrypt (passphrase, passphraselen, algo, subalgo,
                             salt, saltlen, iterations, keysize, keybuffer);
#else
      ec = GPG_ERR_UNSUPPORTED_ALGORITHM;
#endif /*USE_SCRYPT*/
      break;

    default:
      ec = GPG_ERR_UNKNOWN_ALGORITHM;
      break;
    }

 leave:
  return ec;
}

#include "bufhelp.h"

typedef struct argon2_context *argon2_ctx_t;

/* Per thread data for Argon2.  */
struct argon2_thread_data {
  argon2_ctx_t a;
  unsigned int pass;
  unsigned int slice;
  unsigned int lane;
};

/* Argon2 context */
struct argon2_context {
  int algo;
  int hash_type;

  unsigned int outlen;

  const unsigned char *password;
  size_t passwordlen;

  const unsigned char *salt;
  size_t saltlen;

  const unsigned char *key;
  size_t keylen;

  const unsigned char *ad;
  size_t adlen;

  unsigned int m_cost;

  unsigned int passes;
  unsigned int memory_blocks;
  unsigned int segment_length;
  unsigned int lane_length;
  unsigned int lanes;

  u64 *block;
  struct argon2_thread_data *thread_data;

  unsigned char out[1];  /* In future, we may use flexible array member.  */
};

#define ARGON2_VERSION 0x13

#define ARGON2_WORDS_IN_BLOCK (1024/8)

static void
xor_block (u64 *dst, const u64 *src)
{
  int i;

  for (i = 0; i < ARGON2_WORDS_IN_BLOCK; i++)
    dst[i] ^= src[i];
}

static void
beswap64_block (u64 *dst)
{
#ifdef WORDS_BIGENDIAN
  int i;

  /* Swap a block in big-endian 64-bit word into one in
     little-endian.  */
  for (i = 0; i < ARGON2_WORDS_IN_BLOCK; i++)
    dst[i] = _gcry_bswap64 (dst[i]);
#else
  /* Nothing to do.  */
  (void)dst;
#endif
}


static gpg_err_code_t
argon2_fill_first_blocks (argon2_ctx_t a)
{
  unsigned char h0_01_i[72];
  unsigned char buf[10][4];
  gcry_buffer_t iov[8];
  unsigned int iov_count = 0;
  int i;

  /* Generate H0.  */
  buf_put_le32 (buf[0], a->lanes);
  buf_put_le32 (buf[1], a->outlen);
  buf_put_le32 (buf[2], a->m_cost);
  buf_put_le32 (buf[3], a->passes);
  buf_put_le32 (buf[4], ARGON2_VERSION);
  buf_put_le32 (buf[5], a->hash_type);
  buf_put_le32 (buf[6], a->passwordlen);
  iov[iov_count].data = buf[0];
  iov[iov_count].len = 4 * 7;
  iov[iov_count].off = 0;
  iov_count++;
  if (a->passwordlen)
    {
      iov[iov_count].data = (void *)a->password;
      iov[iov_count].len = a->passwordlen;
      iov[iov_count].off = 0;
      iov_count++;
    }

  buf_put_le32 (buf[7], a->saltlen);
  iov[iov_count].data = buf[7];
  iov[iov_count].len = 4;
  iov[iov_count].off = 0;
  iov_count++;
  iov[iov_count].data = (void *)a->salt;
  iov[iov_count].len = a->saltlen;
  iov[iov_count].off = 0;
  iov_count++;

  buf_put_le32 (buf[8], a->keylen);
  iov[iov_count].data = buf[8];
  iov[iov_count].len = 4;
  iov[iov_count].off = 0;
  iov_count++;
  if (a->key)
    {
      iov[iov_count].data = (void *)a->key;
      iov[iov_count].len = a->keylen;
      iov[iov_count].off = 0;
      iov_count++;
    }

  buf_put_le32 (buf[9], a->adlen);
  iov[iov_count].data = buf[9];
  iov[iov_count].len = 4;
  iov[iov_count].off = 0;
  iov_count++;
  if (a->ad)
    {
      iov[iov_count].data = (void *)a->ad;
      iov[iov_count].len = a->adlen;
      iov[iov_count].off = 0;
      iov_count++;
    }

  _gcry_digest_spec_blake2b_512.hash_buffers (h0_01_i, 64, iov, iov_count);

  for (i = 0; i < a->lanes; i++)
    {
      memset (h0_01_i+64, 0, 4);
      buf_put_le32 (h0_01_i+64+4, i);
      blake2b_vl_hash (h0_01_i, 72, 1024,
                       &a->block[i*a->lane_length*ARGON2_WORDS_IN_BLOCK]);
      beswap64_block (&a->block[i*a->lane_length*ARGON2_WORDS_IN_BLOCK]);

      buf_put_le32 (h0_01_i+64, 1);
      blake2b_vl_hash (h0_01_i, 72, 1024,
                       &a->block[(i*a->lane_length+1)*ARGON2_WORDS_IN_BLOCK]);
      beswap64_block (&a->block[(i*a->lane_length+1)*ARGON2_WORDS_IN_BLOCK]);
    }
  return 0;
}

static gpg_err_code_t
argon2_init (argon2_ctx_t a, unsigned int parallelism,
             unsigned int m_cost, unsigned int t_cost)
{
  gpg_err_code_t ec = 0;
  unsigned int memory_blocks;
  unsigned int segment_length;
  void *block;
  struct argon2_thread_data *thread_data;

  memory_blocks = m_cost;
  if (memory_blocks < 8 * parallelism)
    memory_blocks = 8 * parallelism;

  segment_length = memory_blocks / (parallelism * 4);
  memory_blocks = segment_length * parallelism * 4;

  a->passes = t_cost;
  a->memory_blocks = memory_blocks;
  a->segment_length = segment_length;
  a->lane_length = segment_length * 4;
  a->lanes = parallelism;

  a->block = NULL;
  a->thread_data = NULL;

  block = xtrymalloc (1024 * memory_blocks);
  if (!block)
    {
      ec = gpg_err_code_from_errno (errno);
      return ec;
    }
  memset (block, 0, 1024 * memory_blocks);

  thread_data = xtrymalloc (a->lanes * sizeof (struct argon2_thread_data));
  if (!thread_data)
    {
      ec = gpg_err_code_from_errno (errno);
      xfree (block);
      return ec;
    }

  memset (thread_data, 0, a->lanes * sizeof (struct argon2_thread_data));

  a->block = block;
  a->thread_data = thread_data;
  return 0;
}


static u64 fBlaMka (u64 x, u64 y)
{
  const u64 m = U64_C(0xFFFFFFFF);
  return x + y + 2 * (x & m) * (y & m);
}

static u64 rotr64 (u64 w, unsigned int c)
{
  return (w >> c) | (w << (64 - c));
}

#define G(a, b, c, d)                                                          \
    do {                                                                       \
        a = fBlaMka(a, b);                                                     \
        d = rotr64(d ^ a, 32);                                                 \
        c = fBlaMka(c, d);                                                     \
        b = rotr64(b ^ c, 24);                                                 \
        a = fBlaMka(a, b);                                                     \
        d = rotr64(d ^ a, 16);                                                 \
        c = fBlaMka(c, d);                                                     \
        b = rotr64(b ^ c, 63);                                                 \
    } while ((void)0, 0)

#define BLAKE2_ROUND_NOMSG(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11,   \
                           v12, v13, v14, v15)                                 \
    do {                                                                       \
        G(v0, v4, v8, v12);                                                    \
        G(v1, v5, v9, v13);                                                    \
        G(v2, v6, v10, v14);                                                   \
        G(v3, v7, v11, v15);                                                   \
        G(v0, v5, v10, v15);                                                   \
        G(v1, v6, v11, v12);                                                   \
        G(v2, v7, v8, v13);                                                    \
        G(v3, v4, v9, v14);                                                    \
    } while ((void)0, 0)

static void
fill_block (const u64 *prev_block, const u64 *ref_block, u64 *curr_block,
            int with_xor)
{
  u64 block_r[ARGON2_WORDS_IN_BLOCK];
  u64 block_tmp[ARGON2_WORDS_IN_BLOCK];
  int i;

  memcpy (block_r, ref_block, 1024);
  if (prev_block)
    xor_block (block_r, prev_block);
  memcpy (block_tmp, block_r, 1024);

  if (with_xor)
    xor_block (block_tmp, curr_block);

  for (i = 0; i < 8; ++i)
    BLAKE2_ROUND_NOMSG
      (block_r[16 * i],      block_r[16 * i + 1],  block_r[16 * i + 2],
       block_r[16 * i + 3],  block_r[16 * i + 4],  block_r[16 * i + 5],
       block_r[16 * i + 6],  block_r[16 * i + 7],  block_r[16 * i + 8],
       block_r[16 * i + 9],  block_r[16 * i + 10], block_r[16 * i + 11],
       block_r[16 * i + 12], block_r[16 * i + 13], block_r[16 * i + 14],
       block_r[16 * i + 15]);

  for (i = 0; i < 8; i++)
    BLAKE2_ROUND_NOMSG
      (block_r[2 * i],      block_r[2 * i + 1],  block_r[2 * i + 16],
       block_r[2 * i + 17], block_r[2 * i + 32], block_r[2 * i + 33],
       block_r[2 * i + 48], block_r[2 * i + 49], block_r[2 * i + 64],
       block_r[2 * i + 65], block_r[2 * i + 80], block_r[2 * i + 81],
       block_r[2 * i + 96], block_r[2 * i + 97], block_r[2 * i + 112],
       block_r[2 * i + 113]);

  memcpy (curr_block, block_tmp, 1024);
  xor_block (curr_block, block_r);
}

static void
pseudo_random_generate (u64 *random_block, u64 *input_block)
{
  input_block[6]++;
  fill_block (NULL, input_block, random_block, 0);
  fill_block (NULL, random_block, random_block, 0);
}

static u32
index_alpha (argon2_ctx_t a, const struct argon2_thread_data *t,
             int segment_index, u32 random, int same_lane)
{
  u32 reference_area_size;
  u64 relative_position;
  u32 start_position;

  if (t->pass == 0)
    {
      if (t->slice == 0)
        reference_area_size = segment_index - 1;
      else
        {
          if (same_lane)
            reference_area_size = t->slice * a->segment_length
              + segment_index - 1;
          else
            reference_area_size = t->slice * a->segment_length +
              ((segment_index == 0) ? -1 : 0);
        }
    }
  else
    {
      if (same_lane)
        reference_area_size = a->lane_length
          - a->segment_length + segment_index - 1;
      else
        reference_area_size = a->lane_length
          - a->segment_length + ((segment_index == 0) ? -1 : 0);
    }

  relative_position = (random * (u64)random) >> 32;
  relative_position = reference_area_size - 1 -
    ((reference_area_size * relative_position) >> 32);

  if (t->pass == 0)
    start_position = 0;
  else
    start_position = (t->slice == 4 - 1)
      ? 0
      : (t->slice + 1) * a->segment_length;

  return (start_position + relative_position) % a->lane_length;
}

static void
argon2_compute_segment (void *priv)
{
  const struct argon2_thread_data *t = (const struct argon2_thread_data *)priv;
  argon2_ctx_t a = t->a;
  int i;
  int prev_offset, curr_offset;
  u32 ref_index, ref_lane;
  u64 input_block[1024/sizeof (u64)];
  u64 address_block[1024/sizeof (u64)];
  u64 *random_block = NULL;

  if (a->hash_type == GCRY_KDF_ARGON2I
      || (a->hash_type == GCRY_KDF_ARGON2ID && t->pass == 0 && t->slice < 2))
    {
      memset (input_block, 0, 1024);
      input_block[0] = t->pass;
      input_block[1] = t->lane;
      input_block[2] = t->slice;
      input_block[3] = a->memory_blocks;
      input_block[4] = a->passes;
      input_block[5] = a->hash_type;
      random_block = address_block;
    }

  if (t->pass == 0 && t->slice == 0)
    {
      if (random_block)
        pseudo_random_generate (random_block, input_block);
      i = 2;
    }
  else
    i = 0;

  curr_offset = t->lane * a->lane_length + t->slice * a->segment_length + i;
  if ((curr_offset % a->lane_length))
    prev_offset = curr_offset - 1;
  else
    prev_offset = curr_offset + a->lane_length - 1;

  for (; i < a->segment_length; i++, curr_offset++, prev_offset++)
    {
      u64 *ref_block, *curr_block;
      u64 rand64;

      if ((curr_offset % a->lane_length) == 1)
        prev_offset = curr_offset - 1;

      if (random_block)
        {
          if ((i % (1024/sizeof (u64))) == 0)
            pseudo_random_generate (random_block, input_block);

          rand64 = random_block[(i% (1024/sizeof (u64)))];
        }
      else
        rand64 = a->block[prev_offset*ARGON2_WORDS_IN_BLOCK];

      if (t->pass == 0 && t->slice == 0)
        ref_lane = t->lane;
      else
        ref_lane = (rand64 >> 32) % a->lanes;

      ref_index = index_alpha (a, t, i, (rand64 & 0xffffffff),
                               ref_lane == t->lane);
      ref_block =
        &a->block[(a->lane_length * ref_lane + ref_index)* ARGON2_WORDS_IN_BLOCK];

      curr_block = &a->block[curr_offset * ARGON2_WORDS_IN_BLOCK];
      fill_block (&a->block[prev_offset * ARGON2_WORDS_IN_BLOCK], ref_block,
                  curr_block, t->pass != 0);
    }
}


static gpg_err_code_t
argon2_compute (argon2_ctx_t a, const struct gcry_kdf_thread_ops *ops)
{
  gpg_err_code_t ec;
  unsigned int r;
  unsigned int s;
  unsigned int l;
  int ret;

  ec = argon2_fill_first_blocks (a);
  if (ec)
    return ec;

  for (r = 0; r < a->passes; r++)
    for (s = 0; s < 4; s++)
      {
        for (l = 0; l < a->lanes; l++)
          {
            struct argon2_thread_data *thread_data;

            /* launch a thread.  */
            thread_data = &a->thread_data[l];
            thread_data->a = a;
            thread_data->pass = r;
            thread_data->slice = s;
            thread_data->lane = l;

            if (ops)
	      {
		ret = ops->dispatch_job (ops->jobs_context,
					 argon2_compute_segment, thread_data);
		if (ret < 0)
		  return GPG_ERR_CANCELED;
	      }
            else
              argon2_compute_segment (thread_data);
          }

        if (ops)
	  {
	    ret = ops->wait_all_jobs (ops->jobs_context);
	    if (ret < 0)
	      return GPG_ERR_CANCELED;
	  }
      }

  return 0;
}


static gpg_err_code_t
argon2_final (argon2_ctx_t a, size_t resultlen, void *result)
{
  int i;

  if (resultlen != a->outlen)
    return GPG_ERR_INV_VALUE;

  memset (a->block, 0, 1024);
  for (i = 0; i < a->lanes; i++)
    {
      u64 *last_block;

      last_block = &a->block[(a->lane_length * i + (a->lane_length - 1))
                             * ARGON2_WORDS_IN_BLOCK];
      xor_block (a->block, last_block);
    }

  beswap64_block (a->block);
  blake2b_vl_hash (a->block, 1024, a->outlen, result);
  return 0;
}

static void
argon2_close (argon2_ctx_t a)
{
  size_t n;

  n = offsetof (struct argon2_context, out) + a->outlen;

  if (a->block)
    {
      wipememory (a->block, 1024 * a->memory_blocks);
      xfree (a->block);
    }

  if (a->thread_data)
    xfree (a->thread_data);

  wipememory (a, n);
  xfree (a);
}

static gpg_err_code_t
argon2_open (gcry_kdf_hd_t *hd, int subalgo,
             const unsigned long *param, unsigned int paramlen,
             const void *password, size_t passwordlen,
             const void *salt, size_t saltlen,
             const void *key, size_t keylen,
             const void *ad, size_t adlen)
{
  int hash_type;
  unsigned int taglen;
  unsigned int t_cost;
  unsigned int m_cost;
  unsigned int parallelism = 1;
  argon2_ctx_t a;
  gpg_err_code_t ec;
  size_t n;

  if (subalgo != GCRY_KDF_ARGON2D
      && subalgo != GCRY_KDF_ARGON2I
      && subalgo != GCRY_KDF_ARGON2ID)
    return GPG_ERR_INV_VALUE;
  else
    hash_type = subalgo;

  /* param : [ tag_length, t_cost, m_cost, parallelism ] */
  if (paramlen < 3 || paramlen > 4)
    return GPG_ERR_INV_VALUE;
  else
    {
      taglen = (unsigned int)param[0];
      t_cost = (unsigned int)param[1];
      m_cost = (unsigned int)param[2];
      if (paramlen >= 4)
        parallelism = (unsigned int)param[3];
    }

  if (parallelism == 0)
    return GPG_ERR_INV_VALUE;

  n = offsetof (struct argon2_context, out) + taglen;
  a = xtrymalloc (n);
  if (!a)
    return gpg_err_code_from_errno (errno);

  a->algo = GCRY_KDF_ARGON2;
  a->hash_type = hash_type;

  a->outlen = taglen;

  a->password = password;
  a->passwordlen = passwordlen;
  a->salt = salt;
  a->saltlen = saltlen;
  a->key = key;
  a->keylen = keylen;
  a->ad = ad;
  a->adlen = adlen;

  a->m_cost = m_cost;

  a->block = NULL;
  a->thread_data = NULL;

  ec = argon2_init (a, parallelism, m_cost, t_cost);
  if (ec)
    {
      xfree (a);
      return ec;
    }

  *hd = (void *)a;
  return 0;
}

typedef struct balloon_context *balloon_ctx_t;

/* Per thread data for Balloon.  */
struct balloon_thread_data {
  balloon_ctx_t b;
  gpg_err_code_t ec;
  unsigned int idx;
  unsigned char *block;
};

/* Balloon context */
struct balloon_context {
  int algo;
  int prng_type;

  unsigned int blklen;
  const gcry_md_spec_t *md_spec;

  const unsigned char *password;
  size_t passwordlen;

  const unsigned char *salt;
  /* Length of salt is fixed.  */

  unsigned int s_cost;
  unsigned int t_cost;
  unsigned int parallelism;

  u64 n_blocks;

  unsigned char *block;

  /* In future, we may use flexible array member.  */
  struct balloon_thread_data thread_data[1];
};

/* Maximum size of underlining digest size.  */
#define BALLOON_BLOCK_LEN_MAX 64

static gpg_err_code_t
prng_aes_ctr_init (gcry_cipher_hd_t *hd_p, balloon_ctx_t b,
                   gcry_buffer_t *iov, unsigned int iov_count)
{
  gpg_err_code_t ec;
  gcry_cipher_hd_t hd;
  unsigned char key[BALLOON_BLOCK_LEN_MAX];
  int cipher_algo;
  unsigned int keylen, blklen;

  switch (b->blklen)
    {
    case 64:
      cipher_algo = GCRY_CIPHER_AES256;
      break;

    case 48:
      cipher_algo = GCRY_CIPHER_AES192;
      break;

    default:
    case 32:
      cipher_algo = GCRY_CIPHER_AES;
      break;
    }

  keylen = _gcry_cipher_get_algo_keylen (cipher_algo);
  blklen = _gcry_cipher_get_algo_blklen (cipher_algo);

  b->md_spec->hash_buffers (key, b->blklen, iov, iov_count);
  ec = _gcry_cipher_open (&hd, cipher_algo, GCRY_CIPHER_MODE_CTR, 0);
  if (ec)
    return ec;

  ec = _gcry_cipher_setkey (hd, key, keylen);
  if (ec)
    {
      _gcry_cipher_close (hd);
      return ec;
    }

  if (cipher_algo == GCRY_CIPHER_AES
      && b->md_spec == &_gcry_digest_spec_sha256)
    /* Original Balloon uses zero IV.  */
    ;
  else
    {
      ec = _gcry_cipher_setiv (hd, key+keylen, blklen);
      if (ec)
        {
          _gcry_cipher_close (hd);
          return ec;
        }
    }

  wipememory (key, BALLOON_BLOCK_LEN_MAX);
  *hd_p = hd;
  return ec;
}

static u64
prng_aes_ctr_get_rand64 (gcry_cipher_hd_t hd)
{
  static const unsigned char zero64[8];
  unsigned char rand64[8];

  _gcry_cipher_encrypt (hd, rand64, sizeof (rand64), zero64, sizeof (zero64));
  return buf_get_le64 (rand64);
}

static void
prng_aes_ctr_fini (gcry_cipher_hd_t hd)
{
  _gcry_cipher_close (hd);
}

static size_t
ballon_context_size (unsigned int parallelism)
{
  size_t n;

  n = offsetof (struct balloon_context, thread_data)
    + parallelism * sizeof (struct balloon_thread_data);
  return n;
}

static gpg_err_code_t
balloon_open (gcry_kdf_hd_t *hd, int subalgo,
              const unsigned long *param, unsigned int paramlen,
              const void *password, size_t passwordlen,
              const void *salt, size_t saltlen)
{
  unsigned int blklen;
  int hash_type;
  unsigned int s_cost;
  unsigned int t_cost;
  unsigned int parallelism = 1;
  balloon_ctx_t b;
  gpg_err_code_t ec;
  size_t n;
  unsigned char *block;
  unsigned int i;
  const gcry_md_spec_t *md_spec;

  hash_type = subalgo;
  switch (hash_type)
    {
    case GCRY_MD_SHA256:
      md_spec = &_gcry_digest_spec_sha256;
      break;

    case GCRY_MD_SHA384:
      md_spec = &_gcry_digest_spec_sha384;
      break;

    case GCRY_MD_SHA512:
      md_spec = &_gcry_digest_spec_sha512;
      break;

    case GCRY_MD_SHA3_256:
      md_spec = &_gcry_digest_spec_sha3_256;
      break;

    case GCRY_MD_SHA3_384:
      md_spec = &_gcry_digest_spec_sha3_384;
      break;

    case GCRY_MD_SHA3_512:
      md_spec = &_gcry_digest_spec_sha3_512;
      break;

    default:
      return GPG_ERR_NOT_SUPPORTED;
    }

  blklen = _gcry_md_get_algo_dlen (hash_type);
  if (!blklen || blklen > BALLOON_BLOCK_LEN_MAX)
    return GPG_ERR_NOT_SUPPORTED;

  if (saltlen != blklen)
    return GPG_ERR_NOT_SUPPORTED;

  /*
   * It should have space_cost and time_cost.
   * Optionally, for parallelised version, it has parallelism.
   * Possibly (in future), it may have option to specify PRNG type.
   */
  if (paramlen != 2 && paramlen != 3)
    return GPG_ERR_INV_VALUE;
  else
    {
      s_cost = (unsigned int)param[0];
      t_cost = (unsigned int)param[1];
      if (paramlen >= 3)
        parallelism = (unsigned int)param[2];
    }

  if (s_cost < 1)
    return GPG_ERR_INV_VALUE;

  n = ballon_context_size (parallelism);
  b = xtrymalloc (n);
  if (!b)
    return gpg_err_code_from_errno (errno);

  b->algo = GCRY_KDF_BALLOON;
  b->md_spec = md_spec;
  b->blklen = blklen;

  b->password = password;
  b->passwordlen = passwordlen;
  b->salt = salt;

  b->s_cost = s_cost;
  b->t_cost = t_cost;
  b->parallelism = parallelism;

  b->n_blocks = (s_cost * 1024) / b->blklen;

  block = xtrycalloc (parallelism * b->n_blocks, b->blklen);
  if (!block)
    {
      ec = gpg_err_code_from_errno (errno);
      xfree (b);
      return ec;
    }
  b->block = block;

  for (i = 0; i < parallelism; i++)
    {
      struct balloon_thread_data *t = &b->thread_data[i];

      t->b = b;
      t->ec = 0;
      t->idx = i;
      t->block = block;
      block += b->blklen * b->n_blocks;
    }

  *hd = (void *)b;
  return 0;
}


static void
balloon_xor_block (balloon_ctx_t b, u64 *dst, const u64 *src)
{
  int i;

  for (i = 0; i < b->blklen/8; i++)
    dst[i] ^= src[i];
}

#define BALLOON_COMPRESS_BLOCKS 5

static void
balloon_compress (balloon_ctx_t b, u64 *counter_p, unsigned char *out,
                  const unsigned char *blocks[BALLOON_COMPRESS_BLOCKS])
{
  gcry_buffer_t iov[1+BALLOON_COMPRESS_BLOCKS];
  unsigned char octet_counter[sizeof (u64)];
  unsigned int i;

  buf_put_le64 (octet_counter, *counter_p);
  iov[0].data = octet_counter;
  iov[0].len = sizeof (octet_counter);
  iov[0].off = 0;

  for (i = 1; i < 1+BALLOON_COMPRESS_BLOCKS; i++)
    {
      iov[i].data = (void *)blocks[i-1];
      iov[i].len = b->blklen;
      iov[i].off = 0;
    }

  b->md_spec->hash_buffers (out, b->blklen, iov, 1+BALLOON_COMPRESS_BLOCKS);
  *counter_p += 1;
}

static void
balloon_expand (balloon_ctx_t b, u64 *counter_p, unsigned char *block,
                u64 n_blocks)
{
  gcry_buffer_t iov[2];
  unsigned char octet_counter[sizeof (u64)];
  u64 i;

  iov[0].data = octet_counter;
  iov[0].len = sizeof (octet_counter);
  iov[0].off = 0;
  iov[1].len = b->blklen;
  iov[1].off = 0;

  for (i = 1; i < n_blocks; i++)
    {
      buf_put_le64 (octet_counter, *counter_p);
      iov[1].data = block;
      block += b->blklen;
      b->md_spec->hash_buffers (block, b->blklen, iov, 2);
      *counter_p += 1;
    }
}

static void
balloon_compute_fill (balloon_ctx_t b,
                      struct balloon_thread_data *t,
                      const unsigned char *salt,
                      u64 *counter_p)
{
  gcry_buffer_t iov[6];
  unsigned char octet_counter[sizeof (u64)];
  unsigned char octet_s_cost[4];
  unsigned char octet_t_cost[4];
  unsigned char octet_parallelism[4];

  buf_put_le64 (octet_counter, *counter_p);
  buf_put_le32 (octet_s_cost, b->s_cost);
  buf_put_le32 (octet_t_cost, b->t_cost);
  buf_put_le32 (octet_parallelism, b->parallelism);

  iov[0].data = octet_counter;
  iov[0].len = sizeof (octet_counter);
  iov[0].off = 0;
  iov[1].data = (void *)salt;
  iov[1].len = b->blklen;
  iov[1].off = 0;
  iov[2].data = (void *)b->password;
  iov[2].len = b->passwordlen;
  iov[2].off = 0;
  iov[3].data = octet_s_cost;
  iov[3].len = 4;
  iov[3].off = 0;
  iov[4].data = octet_t_cost;
  iov[4].len = 4;
  iov[4].off = 0;
  iov[5].data = octet_parallelism;
  iov[5].len = 4;
  iov[5].off = 0;
  b->md_spec->hash_buffers (t->block, b->blklen, iov, 6);
  *counter_p += 1;
  balloon_expand (b, counter_p, t->block, b->n_blocks);
}

static void
balloon_compute_mix (gcry_cipher_hd_t prng,
                     balloon_ctx_t b, struct balloon_thread_data *t,
                     u64 *counter_p)
{
  u64 i;

  for (i = 0; i < b->n_blocks; i++)
    {
      unsigned char *cur_block = t->block + (b->blklen * i);
      const unsigned char *blocks[BALLOON_COMPRESS_BLOCKS];
      const unsigned char *prev_block;
      unsigned int n;

      prev_block = i
        ? cur_block - b->blklen
        : t->block + (b->blklen * (t->b->n_blocks - 1));

      n = 0;
      blocks[n++] = prev_block;
      blocks[n++] = cur_block;

      for (; n < BALLOON_COMPRESS_BLOCKS; n++)
        {
          u64 rand64 = prng_aes_ctr_get_rand64 (prng);
          blocks[n] = t->block + (b->blklen * (rand64 % b->n_blocks));
        }

      balloon_compress (b, counter_p, cur_block, blocks);
    }
}


static void
balloon_compute (void *priv)
{
  struct balloon_thread_data *t = (struct balloon_thread_data *)priv;
  balloon_ctx_t b = t->b;
  gcry_cipher_hd_t prng;
  gcry_buffer_t iov[4];
  unsigned char salt[BALLOON_BLOCK_LEN_MAX];
  unsigned char octet_s_cost[4];
  unsigned char octet_t_cost[4];
  unsigned char octet_parallelism[4];
  u32 u;
  u64 counter;
  unsigned int i;

  counter = 0;

  memcpy (salt, b->salt, b->blklen);
  u = buf_get_le32 (b->salt) + t->idx;
  buf_put_le32 (salt, u);

  buf_put_le32 (octet_s_cost, b->s_cost);
  buf_put_le32 (octet_t_cost, b->t_cost);
  buf_put_le32 (octet_parallelism, b->parallelism);

  iov[0].data = salt;
  iov[0].len = b->blklen;
  iov[0].off = 0;
  iov[1].data = octet_s_cost;
  iov[1].len = 4;
  iov[1].off = 0;
  iov[2].data = octet_t_cost;
  iov[2].len = 4;
  iov[2].off = 0;
  iov[3].data = octet_parallelism;
  iov[3].len = 4;
  iov[3].off = 0;

  t->ec = prng_aes_ctr_init (&prng, b, iov, 4);
  if (t->ec)
    return;

  balloon_compute_fill (b, t, salt, &counter);

  for (i = 0; i < b->t_cost; i++)
    balloon_compute_mix (prng, b, t, &counter);

  /* The result is now at the last block.  */

  prng_aes_ctr_fini (prng);
}

static gpg_err_code_t
balloon_compute_all (balloon_ctx_t b, const struct gcry_kdf_thread_ops *ops)
{
  unsigned int parallelism = b->parallelism;
  unsigned int i;
  int ret;

  for (i = 0; i < parallelism; i++)
    {
      struct balloon_thread_data *t = &b->thread_data[i];

      if (ops)
        {
          ret = ops->dispatch_job (ops->jobs_context, balloon_compute, t);
          if (ret < 0)
            return GPG_ERR_CANCELED;
        }
      else
        balloon_compute (t);
    }

  if (ops)
    {
      ret = ops->wait_all_jobs (ops->jobs_context);
      if (ret < 0)
        return GPG_ERR_CANCELED;
    }

  return 0;
}

static gpg_err_code_t
balloon_final (balloon_ctx_t b, size_t resultlen, void *result)
{
  unsigned int parallelism = b->parallelism;
  unsigned int i;
  u64 out[BALLOON_BLOCK_LEN_MAX/8];

  if (resultlen != b->blklen)
    return GPG_ERR_INV_VALUE;

  memset (out, 0, b->blklen);
  for (i = 0; i < parallelism; i++)
    {
      struct balloon_thread_data *t = &b->thread_data[i];
      const unsigned char *last_block;

      if (t->ec)
        return t->ec;

      last_block = t->block + (b->blklen * (t->b->n_blocks - 1));
      balloon_xor_block (b, out, (const u64 *)(void *)last_block);
    }

  memcpy (result, out, b->blklen);

  return 0;
}

static void
balloon_close (balloon_ctx_t b)
{
  unsigned int parallelism = b->parallelism;
  size_t n = ballon_context_size (parallelism);

  if (b->block)
    {
      wipememory (b->block, parallelism * b->n_blocks);
      xfree (b->block);
    }

  wipememory (b, n);
  xfree (b);
}

typedef struct onestep_kdf_context *onestep_kdf_ctx_t;

/* OneStepKDF context */
struct onestep_kdf_context {
  int algo;
  gcry_md_hd_t md;
  unsigned int blklen;
  unsigned int outlen;
  const void *input;
  size_t inputlen;
  const void *fixedinfo;
  size_t fixedinfolen;
};

static gpg_err_code_t
onestep_kdf_open (gcry_kdf_hd_t *hd, int hashalgo,
                  const unsigned long *param, unsigned int paramlen,
                  const void *input, size_t inputlen,
                  const void *fixedinfo, size_t fixedinfolen)
{
  gpg_err_code_t ec;
  unsigned int outlen;
  onestep_kdf_ctx_t o;
  size_t n;

  if (paramlen != 1)
    return GPG_ERR_INV_VALUE;
  else
    outlen = (unsigned int)param[0];

  n = sizeof (struct onestep_kdf_context);
  o = xtrymalloc (n);
  if (!o)
    return gpg_err_code_from_errno (errno);

  o->blklen = _gcry_md_get_algo_dlen (hashalgo);
  if (!o->blklen)
    {
      xfree (o);
      return GPG_ERR_DIGEST_ALGO;
    }
  ec = _gcry_md_open (&o->md, hashalgo, 0);
  if (ec)
    {
      xfree (o);
      return ec;
    }
  o->algo = GCRY_KDF_ONESTEP_KDF;
  o->outlen = outlen;
  o->input = input;
  o->inputlen = inputlen;
  o->fixedinfo = fixedinfo;
  o->fixedinfolen = fixedinfolen;

  *hd = (void *)o;
  return 0;
}


static gpg_err_code_t
onestep_kdf_compute (onestep_kdf_ctx_t o, const struct gcry_kdf_thread_ops *ops)
{
  (void)o;

  if (ops != NULL)
    return GPG_ERR_INV_VALUE;

  return 0;
}

static gpg_err_code_t
onestep_kdf_final (onestep_kdf_ctx_t o, size_t resultlen, void *result)
{
  u32 counter = 0;
  unsigned char cnt[4];
  int i;

  if (resultlen != o->outlen)
    return GPG_ERR_INV_VALUE;

  for (i = 0; i < o->outlen / o->blklen; i++)
    {
      counter++;
      buf_put_be32 (cnt, counter);
      _gcry_md_write (o->md, cnt, sizeof (cnt));
      _gcry_md_write (o->md, o->input, o->inputlen);
      _gcry_md_write (o->md, o->fixedinfo, o->fixedinfolen);
      _gcry_md_final (o->md);
      memcpy ((char *)result + o->blklen * i,
              _gcry_md_read (o->md, 0), o->blklen);
      resultlen -= o->blklen;
      _gcry_md_reset (o->md);
    }

  if (resultlen)
    {
      counter++;
      buf_put_be32 (cnt, counter);
      _gcry_md_write (o->md, cnt, sizeof (cnt));
      _gcry_md_write (o->md, o->input, o->inputlen);
      _gcry_md_write (o->md, o->fixedinfo, o->fixedinfolen);
      _gcry_md_final (o->md);
      memcpy ((char *)result + o->blklen * i,
              _gcry_md_read (o->md, 0), resultlen);
    }

  return 0;
}

static void
onestep_kdf_close (onestep_kdf_ctx_t o)
{
  _gcry_md_close (o->md);
  xfree (o);
}

typedef struct onestep_kdf_mac_context *onestep_kdf_mac_ctx_t;

/* OneStep_KDF_MAC context */
struct onestep_kdf_mac_context {
  int algo;
  gcry_mac_hd_t md;
  unsigned int blklen;
  unsigned int outlen;
  const void *input;
  size_t inputlen;
  const void *salt;
  size_t saltlen;
  const void *fixedinfo;
  size_t fixedinfolen;
};

static gpg_err_code_t
onestep_kdf_mac_open (gcry_kdf_hd_t *hd, int macalgo,
                      const unsigned long *param, unsigned int paramlen,
                      const void *input, size_t inputlen,
                      const void *key, size_t keylen,
                      const void *fixedinfo, size_t fixedinfolen)
{
  gpg_err_code_t ec;
  unsigned int outlen;
  onestep_kdf_mac_ctx_t o;
  size_t n;

  if (paramlen != 1)
    return GPG_ERR_INV_VALUE;
  else
    outlen = (unsigned int)param[0];

  n = sizeof (struct onestep_kdf_mac_context);
  o = xtrymalloc (n);
  if (!o)
    return gpg_err_code_from_errno (errno);

  o->blklen = _gcry_mac_get_algo_maclen (macalgo);
  if (!o->blklen)
    {
      xfree (o);
      return GPG_ERR_MAC_ALGO;
    }
  ec = _gcry_mac_open (&o->md, macalgo, 0, NULL);
  if (ec)
    {
      xfree (o);
      return ec;
    }
  o->algo = GCRY_KDF_ONESTEP_KDF_MAC;
  o->outlen = outlen;
  o->input = input;
  o->inputlen = inputlen;
  o->salt = key;
  o->saltlen = keylen;
  o->fixedinfo = fixedinfo;
  o->fixedinfolen = fixedinfolen;

  *hd = (void *)o;
  return 0;
}


static gpg_err_code_t
onestep_kdf_mac_compute (onestep_kdf_mac_ctx_t o,
                         const struct gcry_kdf_thread_ops *ops)
{
  (void)o;

  if (ops != NULL)
    return GPG_ERR_INV_VALUE;

  return 0;
}

static gpg_err_code_t
onestep_kdf_mac_final (onestep_kdf_mac_ctx_t o, size_t resultlen, void *result)
{
  u32 counter = 0;
  unsigned char cnt[4];
  int i;
  gcry_err_code_t ec;
  size_t len = o->blklen;

  if (resultlen != o->outlen)
    return GPG_ERR_INV_VALUE;

  ec = _gcry_mac_setkey (o->md, o->salt, o->saltlen);
  if (ec)
    return ec;

  for (i = 0; i < o->outlen / o->blklen; i++)
    {
      counter++;
      buf_put_be32 (cnt, counter);
      ec = _gcry_mac_write (o->md, cnt, sizeof (cnt));
      if (ec)
        return ec;
      ec = _gcry_mac_write (o->md, o->input, o->inputlen);
      if (ec)
        return ec;
      ec = _gcry_mac_write (o->md, o->fixedinfo, o->fixedinfolen);
      if (ec)
        return ec;
      ec = _gcry_mac_read (o->md, (char *)result + o->blklen * i, &len);
      if (ec)
        return ec;
      resultlen -= o->blklen;
      ec = _gcry_mac_ctl (o->md, GCRYCTL_RESET, NULL, 0);
      if (ec)
        return ec;
    }

  if (resultlen)
    {
      counter++;
      len = resultlen;
      buf_put_be32 (cnt, counter);
      ec = _gcry_mac_write (o->md, cnt, sizeof (cnt));
      if (ec)
        return ec;
      ec = _gcry_mac_write (o->md, o->input, o->inputlen);
      if (ec)
        return ec;
      ec =_gcry_mac_write (o->md, o->fixedinfo, o->fixedinfolen);
      if (ec)
        return ec;
      ec = _gcry_mac_read (o->md, (char *)result + o->blklen * i, &len);
      if (ec)
        return ec;
    }

  return 0;
}

static void
onestep_kdf_mac_close (onestep_kdf_mac_ctx_t o)
{
  _gcry_mac_close (o->md);
  xfree (o);
}

typedef struct hkdf_context *hkdf_ctx_t;

/* Hkdf context */
struct hkdf_context {
  int algo;
  gcry_mac_hd_t md;
  int mode;
  unsigned int blklen;
  unsigned int outlen;
  const void *input;
  size_t inputlen;
  const void *salt;
  size_t saltlen;
  const void *fixedinfo;
  size_t fixedinfolen;
  unsigned char *prk;
};

static gpg_err_code_t
hkdf_open (gcry_kdf_hd_t *hd, int macalgo,
           const unsigned long *param, unsigned int paramlen,
           const void *input, size_t inputlen,
           const void *salt, size_t saltlen,
           const void *fixedinfo, size_t fixedinfolen)
{
  gpg_err_code_t ec;
  unsigned int outlen;
  int mode;
  hkdf_ctx_t h;
  size_t n;
  unsigned char *prk;

  if (paramlen != 1 && paramlen != 2)
    return GPG_ERR_INV_VALUE;
  else
    {
      outlen = (unsigned int)param[0];
      /* MODE: support extract only, expand only: FIXME*/
      if (paramlen == 2)
        mode = (unsigned int)param[1];
      else
        mode = 0;
    }

  n = sizeof (struct hkdf_context);
  h = xtrymalloc (n);
  if (!h)
    return gpg_err_code_from_errno (errno);

  h->blklen = _gcry_mac_get_algo_maclen (macalgo);
  if (!h->blklen)
    {
      xfree (h);
      return GPG_ERR_MAC_ALGO;
    }

  if (outlen > 255 * h->blklen)
    {
      xfree (h);
      return GPG_ERR_INV_VALUE;
    }

  ec = _gcry_mac_open (&h->md, macalgo, 0, NULL);
  if (ec)
    {
      xfree (h);
      return ec;
    }
  prk = xtrymalloc (h->blklen);
  if (!prk)
    {
      _gcry_mac_close (h->md);
      xfree (h);
      return gpg_err_code_from_errno (errno);
    }
  h->prk = prk;
  h->algo = GCRY_KDF_HKDF;
  h->outlen = outlen;
  h->mode = mode;
  h->input = input;
  h->inputlen = inputlen;
  h->salt = salt;
  h->saltlen = saltlen;
  h->fixedinfo = fixedinfo;
  h->fixedinfolen = fixedinfolen;

  *hd = (void *)h;
  return 0;
}


static gpg_err_code_t
hkdf_compute (hkdf_ctx_t h, const struct gcry_kdf_thread_ops *ops)
{
  gcry_err_code_t ec;
  size_t len = h->blklen;

  if (ops != NULL)
    return GPG_ERR_INV_VALUE;

  /* Extract */
  ec = _gcry_mac_setkey (h->md, h->salt, h->saltlen);
  if (ec)
    return ec;

  ec = _gcry_mac_write (h->md, h->input, h->inputlen);
  if (ec)
    return ec;

  ec = _gcry_mac_read (h->md, h->prk, &len);
  if (ec)
    return ec;

  ec = _gcry_mac_ctl (h->md, GCRYCTL_RESET, NULL, 0);
  if (ec)
    return ec;

  return 0;
}

static gpg_err_code_t
hkdf_final (hkdf_ctx_t h, size_t resultlen, void *result)
{
  unsigned char counter = 0;
  int i;
  gcry_err_code_t ec;
  size_t len = h->blklen;

  if (resultlen != h->outlen)
    return GPG_ERR_INV_VALUE;

  /* Expand */
  ec = _gcry_mac_setkey (h->md, h->prk, h->blklen);
  if (ec)
    return ec;

  /* We re-use the memory of ->prk.  */

  for (i = 0; i < h->outlen / h->blklen; i++)
    {
      counter++;
      if (i)
        {
          ec = _gcry_mac_write (h->md, h->prk, h->blklen);
          if (ec)
            return ec;
        }
      if (h->fixedinfo)
        {
          ec = _gcry_mac_write (h->md, h->fixedinfo, h->fixedinfolen);
          if (ec)
            return ec;
        }
      ec = _gcry_mac_write (h->md, &counter, 1);
      if (ec)
        return ec;
      ec = _gcry_mac_read (h->md, h->prk, &len);
      if (ec)
        return ec;
      memcpy ((char *)result + h->blklen * i, h->prk, len);
      resultlen -= h->blklen;
      ec = _gcry_mac_ctl (h->md, GCRYCTL_RESET, NULL, 0);
      if (ec)
        return ec;
    }

  if (resultlen)
    {
      counter++;
      len = resultlen;
      if (i)
        {
          ec = _gcry_mac_write (h->md, h->prk, h->blklen);
          if (ec)
            return ec;
        }
      if (h->fixedinfo)
        {
          ec = _gcry_mac_write (h->md, h->fixedinfo, h->fixedinfolen);
          if (ec)
            return ec;
        }
      ec = _gcry_mac_write (h->md, &counter, 1);
      if (ec)
        return ec;
      ec = _gcry_mac_read (h->md, (char *)result + h->blklen * i, &len);
      if (ec)
        return ec;
    }

  return 0;
}

static void
hkdf_close (hkdf_ctx_t h)
{
  _gcry_mac_close (h->md);
  xfree (h->prk);
  xfree (h);
}

typedef struct x963_kdf_context *x963_kdf_ctx_t;

/* X963KDF context */
struct x963_kdf_context {
  int algo;
  gcry_md_hd_t md;
  unsigned int blklen;
  unsigned int outlen;
  const void *input;
  size_t inputlen;
  const void *sharedinfo;
  size_t sharedinfolen;
};

static gpg_err_code_t
x963_kdf_open (gcry_kdf_hd_t *hd, int hashalgo,
                  const unsigned long *param, unsigned int paramlen,
                  const void *input, size_t inputlen,
                  const void *sharedinfo, size_t sharedinfolen)
{
  gpg_err_code_t ec;
  unsigned int outlen;
  x963_kdf_ctx_t o;
  size_t n;

  if (paramlen != 1)
    return GPG_ERR_INV_VALUE;
  else
    outlen = (unsigned int)param[0];

  n = sizeof (struct x963_kdf_context);
  o = xtrymalloc (n);
  if (!o)
    return gpg_err_code_from_errno (errno);

  o->blklen = _gcry_md_get_algo_dlen (hashalgo);
  if (!o->blklen)
    {
      xfree (o);
      return GPG_ERR_DIGEST_ALGO;
    }
  ec = _gcry_md_open (&o->md, hashalgo, 0);
  if (ec)
    {
      xfree (o);
      return ec;
    }
  o->algo = GCRY_KDF_X963_KDF;
  o->outlen = outlen;
  o->input = input;
  o->inputlen = inputlen;
  o->sharedinfo = sharedinfo;
  o->sharedinfolen = sharedinfolen;

  *hd = (void *)o;
  return 0;
}


static gpg_err_code_t
x963_kdf_compute (x963_kdf_ctx_t o, const struct gcry_kdf_thread_ops *ops)
{
  (void)o;

  if (ops != NULL)
    return GPG_ERR_INV_VALUE;

  return 0;
}

static gpg_err_code_t
x963_kdf_final (x963_kdf_ctx_t o, size_t resultlen, void *result)
{
  u32 counter = 0;
  unsigned char cnt[4];
  int i;

  if (resultlen != o->outlen)
    return GPG_ERR_INV_VALUE;

  for (i = 0; i < o->outlen / o->blklen; i++)
    {
      counter++;
      _gcry_md_write (o->md, o->input, o->inputlen);
      buf_put_be32 (cnt, counter);
      _gcry_md_write (o->md, cnt, sizeof (cnt));
      if (o->sharedinfolen)
        _gcry_md_write (o->md, o->sharedinfo, o->sharedinfolen);
      _gcry_md_final (o->md);
      memcpy ((char *)result + o->blklen * i,
              _gcry_md_read (o->md, 0), o->blklen);
      resultlen -= o->blklen;
      _gcry_md_reset (o->md);
    }

  if (resultlen)
    {
      counter++;
      _gcry_md_write (o->md, o->input, o->inputlen);
      buf_put_be32 (cnt, counter);
      _gcry_md_write (o->md, cnt, sizeof (cnt));
      if (o->sharedinfolen)
        _gcry_md_write (o->md, o->sharedinfo, o->sharedinfolen);
      _gcry_md_final (o->md);
      memcpy ((char *)result + o->blklen * i,
              _gcry_md_read (o->md, 0), resultlen);
    }

  return 0;
}

static void
x963_kdf_close (x963_kdf_ctx_t o)
{
  _gcry_md_close (o->md);
  xfree (o);
}

struct gcry_kdf_handle {
  int algo;
  /* And algo specific parts come.  */
};

gpg_err_code_t
_gcry_kdf_open (gcry_kdf_hd_t *hd, int algo, int subalgo,
                const unsigned long *param, unsigned int paramlen,
                const void *input, size_t inputlen,
                const void *salt, size_t saltlen,
                const void *key, size_t keylen,
                const void *ad, size_t adlen)
{
  gpg_err_code_t ec;

  switch (algo)
    {
    case GCRY_KDF_ARGON2:
      if (!saltlen)
        ec = GPG_ERR_INV_VALUE;
      else
        ec = argon2_open (hd, subalgo, param, paramlen,
                          input, inputlen, salt, saltlen,
                          key, keylen, ad, adlen);
      break;

    case GCRY_KDF_BALLOON:
      if (!inputlen || !saltlen || keylen || adlen)
        ec = GPG_ERR_INV_VALUE;
      else
        {
          (void)key;
          (void)ad;
          ec = balloon_open (hd, subalgo, param, paramlen,
                             input, inputlen, salt, saltlen);
        }
      break;

    case GCRY_KDF_ONESTEP_KDF:
      if (!inputlen || !paramlen || !adlen)
        ec = GPG_ERR_INV_VALUE;
      else
        {
          (void)salt;
          (void)key;
          ec = onestep_kdf_open (hd, subalgo, param, paramlen,
                                 input, inputlen, ad, adlen);
        }
      break;

    case GCRY_KDF_ONESTEP_KDF_MAC:
      if (!inputlen || !paramlen || !keylen || !adlen)
        ec = GPG_ERR_INV_VALUE;
      else
        {
          (void)salt;
          ec = onestep_kdf_mac_open (hd, subalgo, param, paramlen,
                                     input, inputlen, key, keylen, ad, adlen);
        }
      break;

    case GCRY_KDF_HKDF:
      if (!inputlen || !paramlen)
        ec = GPG_ERR_INV_VALUE;
      else
        {
          (void)salt;
          ec = hkdf_open (hd, subalgo, param, paramlen,
                          input, inputlen, key, keylen, ad, adlen);
        }
      break;

    case GCRY_KDF_X963_KDF:
      if (!inputlen || !paramlen)
        ec = GPG_ERR_INV_VALUE;
      else
        {
          (void)salt;
          (void)key;
          ec = x963_kdf_open (hd, subalgo, param, paramlen,
                              input, inputlen, ad, adlen);
        }
      break;

    default:
      ec = GPG_ERR_UNKNOWN_ALGORITHM;
      break;
    }

  return ec;
}

gpg_err_code_t
_gcry_kdf_compute (gcry_kdf_hd_t h, const struct gcry_kdf_thread_ops *ops)
{
  gpg_err_code_t ec;

  switch (h->algo)
    {
    case GCRY_KDF_ARGON2:
      ec = argon2_compute ((argon2_ctx_t)(void *)h, ops);
      break;

    case GCRY_KDF_BALLOON:
      ec = balloon_compute_all ((balloon_ctx_t)(void *)h, ops);
      break;

    case GCRY_KDF_ONESTEP_KDF:
      ec = onestep_kdf_compute ((onestep_kdf_ctx_t)(void *)h, ops);
      break;

    case GCRY_KDF_ONESTEP_KDF_MAC:
      ec = onestep_kdf_mac_compute ((onestep_kdf_mac_ctx_t)(void *)h, ops);
      break;

    case GCRY_KDF_HKDF:
      ec = hkdf_compute ((hkdf_ctx_t)(void *)h, ops);
      break;

    case GCRY_KDF_X963_KDF:
      ec = x963_kdf_compute ((x963_kdf_ctx_t)(void *)h, ops);
      break;

    default:
      ec = GPG_ERR_UNKNOWN_ALGORITHM;
      break;
    }

  return ec;
}


gpg_err_code_t
_gcry_kdf_final (gcry_kdf_hd_t h, size_t resultlen, void *result)
{
  gpg_err_code_t ec;

  switch (h->algo)
    {
    case GCRY_KDF_ARGON2:
      ec = argon2_final ((argon2_ctx_t)(void *)h, resultlen, result);
      break;

    case GCRY_KDF_BALLOON:
      ec = balloon_final ((balloon_ctx_t)(void *)h, resultlen, result);
      break;

    case GCRY_KDF_ONESTEP_KDF:
      ec = onestep_kdf_final ((onestep_kdf_ctx_t)(void *)h, resultlen, result);
      break;

    case GCRY_KDF_ONESTEP_KDF_MAC:
      ec = onestep_kdf_mac_final ((onestep_kdf_mac_ctx_t)(void *)h,
                                  resultlen, result);
      break;

    case GCRY_KDF_HKDF:
      ec = hkdf_final ((hkdf_ctx_t)(void *)h, resultlen, result);
      break;

    case GCRY_KDF_X963_KDF:
      ec = x963_kdf_final ((x963_kdf_ctx_t)(void *)h, resultlen, result);
      break;

    default:
      ec = GPG_ERR_UNKNOWN_ALGORITHM;
      break;
    }

  return ec;
}

void
_gcry_kdf_close (gcry_kdf_hd_t h)
{
  switch (h->algo)
    {
    case GCRY_KDF_ARGON2:
      argon2_close ((argon2_ctx_t)(void *)h);
      break;

    case GCRY_KDF_BALLOON:
      balloon_close ((balloon_ctx_t)(void *)h);
      break;

    case GCRY_KDF_ONESTEP_KDF:
      onestep_kdf_close ((onestep_kdf_ctx_t)(void *)h);
      break;

    case GCRY_KDF_ONESTEP_KDF_MAC:
      onestep_kdf_mac_close ((onestep_kdf_mac_ctx_t)(void *)h);
      break;

    case GCRY_KDF_HKDF:
      hkdf_close ((hkdf_ctx_t)(void *)h);
      break;

    case GCRY_KDF_X963_KDF:
      x963_kdf_close ((x963_kdf_ctx_t)(void *)h);
      break;

    default:
      break;
    }
}

/* Check one KDF call with ALGO and HASH_ALGO using the regular KDF
 * API. (passphrase,passphraselen) is the password to be derived,
 * (salt,saltlen) the salt for the key derivation,
 * iterations is the number of the kdf iterations,
 * and (expect,expectlen) the expected result. Returns NULL on
 * success or a string describing the failure.  */

static const char *
check_one (int algo, int hash_algo,
           const void *passphrase, size_t passphraselen,
           const void *salt, size_t saltlen,
           unsigned long iterations,
           const void *expect, size_t expectlen)
{
  unsigned char key[512]; /* hardcoded to avoid allocation */
  size_t keysize = expectlen;
  int rv;

  if (keysize > sizeof(key))
    return "invalid tests data";

  rv = _gcry_kdf_derive (passphrase, passphraselen, algo,
                         hash_algo, salt, saltlen, iterations,
                         keysize, key);
  /* In fips mode we have special requirements for the input and
   * output parameters */
  if (fips_mode ())
    {
      if (rv && (passphraselen < 8 || saltlen < 16 ||
                 iterations < 1000 || expectlen < 14))
        return NULL;
      else if (rv)
        return "gcry_kdf_derive unexpectedly failed in FIPS Mode";
    }
  else if (rv)
    return "gcry_kdf_derive failed";

  if (memcmp (key, expect, expectlen))
    return "does not match";

  return NULL;
}


static gpg_err_code_t
selftest_pbkdf2 (int extended, selftest_report_func_t report)
{
  static const struct {
    const char *desc;
    const char *p;   /* Passphrase.  */
    size_t plen;     /* Length of P. */
    const char *salt;
    size_t saltlen;
    int hashalgo;
    unsigned long c; /* Iterations.  */
    int dklen;       /* Requested key length.  */
    const char *dk;  /* Derived key.  */
    int disabled;
  } tv[] = {
#if USE_SHA1
#define NUM_TEST_VECTORS 9
    /* SHA1 test vectors are from RFC-6070.  */
    {
      "Basic PBKDF2 SHA1 #1",
      "password", 8,
      "salt", 4,
      GCRY_MD_SHA1,
      1,
      20,
      "\x0c\x60\xc8\x0f\x96\x1f\x0e\x71\xf3\xa9"
      "\xb5\x24\xaf\x60\x12\x06\x2f\xe0\x37\xa6"
    },
    {
      "Basic PBKDF2 SHA1 #2",
      "password", 8,
      "salt", 4,
      GCRY_MD_SHA1,
      2,
      20,
      "\xea\x6c\x01\x4d\xc7\x2d\x6f\x8c\xcd\x1e"
      "\xd9\x2a\xce\x1d\x41\xf0\xd8\xde\x89\x57"
    },
    {
      "Basic PBKDF2 SHA1 #3",
      "password", 8,
      "salt", 4,
      GCRY_MD_SHA1,
      4096,
      20,
      "\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad"
      "\x49\xd9\x26\xf7\x21\xd0\x65\xa4\x29\xc1"
    },
    {
      "Basic PBKDF2 SHA1 #4",
      "password", 8,
      "salt", 4,
      GCRY_MD_SHA1,
      16777216,
      20,
      "\xee\xfe\x3d\x61\xcd\x4d\xa4\xe4\xe9\x94"
      "\x5b\x3d\x6b\xa2\x15\x8c\x26\x34\xe9\x84",
      1 /* This test takes too long.  */
    },
    {
      "Basic PBKDF2 SHA1 #5",
      "passwordPASSWORDpassword", 24,
      "saltSALTsaltSALTsaltSALTsaltSALTsalt", 36,
      GCRY_MD_SHA1,
      4096,
      25,
      "\x3d\x2e\xec\x4f\xe4\x1c\x84\x9b\x80\xc8"
      "\xd8\x36\x62\xc0\xe4\x4a\x8b\x29\x1a\x96"
      "\x4c\xf2\xf0\x70\x38"
    },
    {
      "Basic PBKDF2 SHA1 #6",
      "pass\0word", 9,
      "sa\0lt", 5,
      GCRY_MD_SHA1,
      4096,
      16,
      "\x56\xfa\x6a\xa7\x55\x48\x09\x9d\xcc\x37"
      "\xd7\xf0\x34\x25\xe0\xc3"
    },
    { /* empty password test, not in RFC-6070 */
      "Basic PBKDF2 SHA1 #7",
      "", 0,
      "salt", 4,
      GCRY_MD_SHA1,
      2,
      20,
      "\x13\x3a\x4c\xe8\x37\xb4\xd2\x52\x1e\xe2"
      "\xbf\x03\xe1\x1c\x71\xca\x79\x4e\x07\x97"
    },
#else
#define NUM_TEST_VECTORS 2
#endif
    {
      "Basic PBKDF2 SHA256",
      "password", 8,
      "salt", 4,
      GCRY_MD_SHA256,
      2,
      32,
      "\xae\x4d\x0c\x95\xaf\x6b\x46\xd3\x2d\x0a\xdf\xf9\x28\xf0\x6d\xd0"
      "\x2a\x30\x3f\x8e\xf3\xc2\x51\xdf\xd6\xe2\xd8\x5a\x95\x47\x4c\x43"
    },
    {
      "Extended PBKDF2 SHA256",
      "passwordPASSWORDpassword", 24,
      "saltSALTsaltSALTsaltSALTsaltSALTsalt", 36,
      GCRY_MD_SHA256,
      4096,
      40,
      "\x34\x8c\x89\xdb\xcb\xd3\x2b\x2f\x32\xd8\x14\xb8\x11\x6e\x84\xcf"
      "\x2b\x17\x34\x7e\xbc\x18\x00\x18\x1c\x4e\x2a\x1f\xb8\xdd\x53\xe1"
      "\xc6\x35\x51\x8c\x7d\xac\x47\xe9"
    },
    { NULL }
  };
  const char *what;
  const char *errtxt;
  int tvidx;

  for (tvidx=0; tv[tvidx].desc; tvidx++)
    {
      what = tv[tvidx].desc;
      if (tv[tvidx].disabled)
        continue;
      errtxt = check_one (GCRY_KDF_PBKDF2, tv[tvidx].hashalgo,
                          tv[tvidx].p, tv[tvidx].plen,
                          tv[tvidx].salt, tv[tvidx].saltlen,
                          tv[tvidx].c,
                          tv[tvidx].dk, tv[tvidx].dklen);
      if (errtxt)
        goto failed;
      if (tvidx >= NUM_TEST_VECTORS - 1 && !extended)
        break;
    }

  return 0; /* Succeeded. */

 failed:
  if (report)
    report ("kdf", GCRY_KDF_PBKDF2, what, errtxt);
  return GPG_ERR_SELFTEST_FAILED;
}


/* Run the selftests for KDF with KDF algorithm ALGO with optional
   reporting function REPORT.  */
gpg_error_t
_gcry_kdf_selftest (int algo, int extended, selftest_report_func_t report)
{
  gcry_err_code_t ec = 0;

  if (algo == GCRY_KDF_PBKDF2)
    ec = selftest_pbkdf2 (extended, report);
  else
    {
      ec = GPG_ERR_UNSUPPORTED_ALGORITHM;
      if (report)
        report ("kdf", algo, "module", "algorithm not available");
    }
  return gpg_error (ec);
}
