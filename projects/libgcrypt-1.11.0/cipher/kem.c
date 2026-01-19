/* kem.c  - Key Encapsulation Mechanisms
 * Copyright (C) 2023 Simon Josefsson <simon@josefsson.org>
 * Copyright (C) 2023 g10 Code GmbH
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "g10lib.h"
#include "cipher.h"
#include "sntrup761.h"
#include "mceliece6688128f.h"
#include "kyber.h"
#include "kem-ecc.h"


/* Information about the the KEM algoithms for use by the s-expression
 * interface.  */
static const struct
{
  const char *name;           /* Name of the algo.  */
  unsigned int namelen;       /* Only here to avoid strlen calls.  */
  int algo;                   /* KEM algo number.   */
  unsigned int nbits;         /* Number of bits.    */
  unsigned int fips:1;        /* True if this is a FIPS140-3 approved KEM. */
  int pubkey_len;             /* Length of the public key.  */
  int seckey_len;             /* Length of the secret key.  */
} kem_infos[] =
  {
    { "sntrup761", 9, GCRY_KEM_SNTRUP761,  761, 0,
      GCRY_KEM_SNTRUP761_PUBKEY_LEN, GCRY_KEM_SNTRUP761_SECKEY_LEN },
    { "kyber512",  8, GCRY_KEM_MLKEM512,   512, 0,
      GCRY_KEM_MLKEM512_PUBKEY_LEN,  GCRY_KEM_MLKEM512_SECKEY_LEN },
    { "kyber768",  8, GCRY_KEM_MLKEM768,   768, 1,
      GCRY_KEM_MLKEM768_PUBKEY_LEN,  GCRY_KEM_MLKEM768_SECKEY_LEN },
    { "kyber1024", 9, GCRY_KEM_MLKEM1024, 1024, 1,
      GCRY_KEM_MLKEM1024_PUBKEY_LEN, GCRY_KEM_MLKEM1024_SECKEY_LEN },
    { NULL }
  };

/* This is a short version of kem_infos from above.  It is required
 * for the algoithm module interface.  Keep in sync.  */
static const char *kem_names[] =
  {
    "sntrup761",
    "kyber512",
    "kyber768",
    "kyber1024",
    NULL
  };




/* Helper for sntrup761.  */
static void
sntrup761_random (void *ctx, size_t length, uint8_t *dst)
{
  (void)ctx;

  _gcry_randomize (dst, length, GCRY_STRONG_RANDOM);
}


gcry_err_code_t
_gcry_kem_keypair (int algo,
                   void *pubkey, size_t pubkey_len,
                   void *seckey, size_t seckey_len)
{
  switch (algo)
    {
    case GCRY_KEM_SNTRUP761:
      if (seckey_len != GCRY_KEM_SNTRUP761_SECKEY_LEN
          || pubkey_len != GCRY_KEM_SNTRUP761_PUBKEY_LEN)
        return GPG_ERR_INV_ARG;
      sntrup761_keypair (pubkey, seckey, NULL, sntrup761_random);
      return 0;

    case GCRY_KEM_CM6688128F:
      mceliece6688128f_keypair (pubkey, seckey);
      return 0;

    case GCRY_KEM_MLKEM512:
      if (seckey_len != GCRY_KEM_MLKEM512_SECKEY_LEN
          || pubkey_len != GCRY_KEM_MLKEM512_PUBKEY_LEN)
        return GPG_ERR_INV_ARG;
      kyber_keypair (algo, pubkey, seckey);
      return 0;

    case GCRY_KEM_MLKEM768:
      if (seckey_len != GCRY_KEM_MLKEM768_SECKEY_LEN
          || pubkey_len != GCRY_KEM_MLKEM768_PUBKEY_LEN)
        return GPG_ERR_INV_ARG;
      kyber_keypair (algo, pubkey, seckey);
      return 0;

    case GCRY_KEM_MLKEM1024:
      if (seckey_len != GCRY_KEM_MLKEM1024_SECKEY_LEN
          || pubkey_len != GCRY_KEM_MLKEM1024_PUBKEY_LEN)
        return GPG_ERR_INV_ARG;
      kyber_keypair (algo, pubkey, seckey);
      return 0;

    case GCRY_KEM_RAW_X25519:
    case GCRY_KEM_RAW_X448:
    case GCRY_KEM_RAW_BP256:
    case GCRY_KEM_RAW_BP384:
    case GCRY_KEM_RAW_BP512:
    case GCRY_KEM_RAW_P256R1:
    case GCRY_KEM_RAW_P384R1:
    case GCRY_KEM_RAW_P521R1:
    case GCRY_KEM_DHKEM25519:
    case GCRY_KEM_DHKEM448:
      return _gcry_ecc_raw_keypair (algo, pubkey, pubkey_len,
                                    seckey, seckey_len);

    default:
      return GPG_ERR_UNKNOWN_ALGORITHM;
    }

  return GPG_ERR_UNKNOWN_ALGORITHM;
}


gcry_err_code_t
_gcry_kem_encap (int algo,
                 const void *pubkey, size_t pubkey_len,
                 void *ciphertext, size_t ciphertext_len,
                 void *shared, size_t shared_len,
                 const void *optional, size_t optional_len)
{
  switch (algo)
    {
    case GCRY_KEM_SNTRUP761:
      if (optional != NULL || optional_len != 0)
        return GPG_ERR_INV_VALUE;
      if (pubkey_len != GCRY_KEM_SNTRUP761_PUBKEY_LEN
          || ciphertext_len != GCRY_KEM_SNTRUP761_ENCAPS_LEN
          || shared_len != GCRY_KEM_SNTRUP761_SHARED_LEN)
        return GPG_ERR_INV_VALUE;
      sntrup761_enc (ciphertext, shared, pubkey, NULL, sntrup761_random);
      return 0;

    case GCRY_KEM_CM6688128F:
      if (optional != NULL)
	return GPG_ERR_INV_VALUE;
      mceliece6688128f_enc (ciphertext, shared, pubkey);
      return 0;

    case GCRY_KEM_MLKEM512:
    case GCRY_KEM_MLKEM768:
    case GCRY_KEM_MLKEM1024:
      if (optional != NULL)
        return GPG_ERR_INV_VALUE;
      kyber_encap (algo, ciphertext, shared, pubkey);
      return 0;

    case GCRY_KEM_RAW_X25519:
    case GCRY_KEM_RAW_X448:
    case GCRY_KEM_RAW_BP256:
    case GCRY_KEM_RAW_BP384:
    case GCRY_KEM_RAW_BP512:
    case GCRY_KEM_RAW_P256R1:
    case GCRY_KEM_RAW_P384R1:
    case GCRY_KEM_RAW_P521R1:
      if (optional != NULL)
        return GPG_ERR_INV_VALUE;
      return _gcry_ecc_raw_encap (algo, pubkey, pubkey_len,
                                  ciphertext, ciphertext_len,
                                  shared, shared_len);

    case GCRY_KEM_DHKEM25519:
    case GCRY_KEM_DHKEM448:
      if (optional != NULL)
        return GPG_ERR_INV_VALUE;
      return _gcry_ecc_dhkem_encap (algo, pubkey, ciphertext, shared);

    default:
      return GPG_ERR_UNKNOWN_ALGORITHM;
    }
  return GPG_ERR_UNKNOWN_ALGORITHM;
}


gcry_err_code_t
_gcry_kem_decap (int algo,
                 const void *seckey, size_t seckey_len,
                 const void *ciphertext, size_t ciphertext_len,
                 void *shared, size_t shared_len,
                 const void *optional, size_t optional_len)
{
  switch (algo)
    {
    case GCRY_KEM_SNTRUP761:
      if (optional != NULL || optional_len != 0)
        return GPG_ERR_INV_VALUE;
      if (seckey_len != GCRY_KEM_SNTRUP761_SECKEY_LEN
          || ciphertext_len != GCRY_KEM_SNTRUP761_ENCAPS_LEN
          || shared_len != GCRY_KEM_SNTRUP761_SHARED_LEN)
        return GPG_ERR_INV_VALUE;
      sntrup761_dec (shared, ciphertext, seckey);
      return 0;

    case GCRY_KEM_CM6688128F:
      if (optional != NULL)
	return GPG_ERR_INV_VALUE;
      mceliece6688128f_dec (shared, ciphertext, seckey);
      return 0;

    case GCRY_KEM_MLKEM512:
    case GCRY_KEM_MLKEM768:
    case GCRY_KEM_MLKEM1024:
      if (optional != NULL)
        return GPG_ERR_INV_VALUE;
      kyber_decap (algo, shared, ciphertext, seckey);
      return 0;

    case GCRY_KEM_RAW_X25519:
    case GCRY_KEM_RAW_X448:
    case GCRY_KEM_RAW_BP256:
    case GCRY_KEM_RAW_BP384:
    case GCRY_KEM_RAW_BP512:
    case GCRY_KEM_RAW_P256R1:
    case GCRY_KEM_RAW_P384R1:
    case GCRY_KEM_RAW_P521R1:
      if (optional != NULL)
        return GPG_ERR_INV_VALUE;
      return _gcry_ecc_raw_decap (algo, seckey, seckey_len,
                                  ciphertext, ciphertext_len,
                                  shared, shared_len);

    case GCRY_KEM_DHKEM25519:
    case GCRY_KEM_DHKEM448:
      return _gcry_ecc_dhkem_decap (algo, seckey, ciphertext, shared,
                                    optional);

    default:
      return GPG_ERR_UNKNOWN_ALGORITHM;
    }
  return GPG_ERR_UNKNOWN_ALGORITHM;
}



/* Generate a KEM keypair using the s-expression interface.  The
 * GENPARAMS is prety simple in this case because it has only the
 * algorithm name.  For example:
 *   (kyber768)
 */
static gcry_err_code_t
kem_generate (const gcry_sexp_t genparms, gcry_sexp_t *r_skey)
{
  gpg_err_code_t ec;
  const char *algo;
  size_t algolen;
  const char *name;
  int i;
  int algoid;
  void *pubkey = NULL;
  void *seckey = NULL;
  size_t pubkey_len, seckey_len;

  algo = sexp_nth_data (genparms, 0, &algolen);
  if (!algo || !algolen)
    return GPG_ERR_PUBKEY_ALGO;
  for (i=0; (name=kem_infos[i].name); i++)
    if (kem_infos[i].namelen == algolen && !memcmp (name, algo, algolen))
      break;
  if (!name)
    return GPG_ERR_WRONG_PUBKEY_ALGO;
  algoid = kem_infos[i].algo;
  pubkey_len = kem_infos[i].pubkey_len;
  seckey_len = kem_infos[i].seckey_len;
  /* (from here on we can jump to leave for cleanup)  */

  /* Allocate buffers for the created key.  */
  seckey = xtrycalloc_secure (1, seckey_len);
  if (!seckey)
    {
      ec = gpg_err_code_from_syserror ();
      goto leave;
    }
  pubkey = xtrycalloc (1, pubkey_len);
  if (!pubkey)
    {
      ec = gpg_err_code_from_syserror ();
      goto leave;
    }

  /* Generate key.  */
  ec = _gcry_kem_keypair (algoid, pubkey, pubkey_len, seckey, seckey_len);
  if (ec)
    goto leave;

  /* Put the key into an s-expression.  */
  ec = sexp_build (r_skey, NULL,
                   "(key-data"
                   " (public-key"
                   "  (%s(p%b)))"
                   " (private-key"
                   "  (%s(p%b)(s%b))))",
                   name,
                   (int)pubkey_len, pubkey,
                   name,
                   (int)pubkey_len, pubkey,
                   (int)seckey_len, seckey);


  /* FIXME: Add FIPS selftest.  */

 leave:
  if (seckey)
    {
      wipememory (seckey, seckey_len);
      xfree (seckey);
    }
  xfree (pubkey);
  return ec;
}


/* Compute a keygrip.  MD is the hash context which we are going to
 * update.  KEYPARAM is an S-expression with the key parameters, this
 * is usually a public key but may also be a secret key.  An example
 * of such an S-expression is:
 *
 *     (kyber768
 *       (p #4243...#)
 *       (s #1718...#))
 *
 * What we hash is the algorithm name, \x00 and the value of p.
 * Including the algorithm name allows us to see a different key
 * despite that it uses the same parameters.  Whether this is a good
 * decision is not clear - but it should not harm.
 */
static gpg_err_code_t
kem_compute_keygrip (gcry_md_hd_t md, gcry_sexp_t keyparam)
{
  gcry_sexp_t l1;
  const char *algo, *data;
  size_t algolen, datalen;
  const char *name;
  int i;

  algo = sexp_nth_data (keyparam, 0, &algolen);
  if (!algo || !algolen)
    return GPG_ERR_PUBKEY_ALGO;
  for (i=0; (name=kem_infos[i].name); i++)
    if (kem_infos[i].namelen == algolen && !memcmp (name, algo, algolen))
      break;
  if (!name)
    return GPG_ERR_WRONG_PUBKEY_ALGO;

  _gcry_md_write (md, name, algolen+1); /* (also hash the nul) */

  l1 = sexp_find_token (keyparam, "p", 1);
  if (!l1)
    return GPG_ERR_NO_OBJ;

  data = sexp_nth_data (l1, 1, &datalen);
  if (!data)
    {
      sexp_release (l1);
      return GPG_ERR_NO_OBJ;
    }

  _gcry_md_write (md, data, datalen);
  sexp_release (l1);

  return 0;
}


/* Return the number of bits for the key described by PARMS.  On error
 * 0 is returned. */
static unsigned int
kem_get_nbits (gcry_sexp_t keyparam)
{
  const char *algo;
  size_t algolen;
  const char *name;
  int i;

  algo = sexp_nth_data (keyparam, 0, &algolen);
  if (!algo || !algolen)
    return 0;  /* GPG_ERR_PUBKEY_ALGO */
  for (i=0; (name=kem_infos[i].name); i++)
    if (kem_infos[i].namelen == algolen && !memcmp (name, algo, algolen))
      break;
  if (!name)
    return 0;  /* GPG_ERR_WRONG_PUBKEY_ALGO */

  return kem_infos[i].nbits;
}


/* Generic structure to represent some KEM algorithms in our public
 * key system.  */
gcry_pk_spec_t _gcry_pubkey_spec_kem =
  {
    GCRY_PK_KEM, { 0, 0 },
    GCRY_PK_USAGE_ENCR,
    "KEM", kem_names,
    "p", "s", "k", "", "p",
    kem_generate,
    NULL,  /* kem_check_secret_key */
    NULL,  /* encrypt_raw - Use gcry_kem_encap instead.  */
    NULL,  /* decrypt_raw - Use gcry_kem_decap unstead.  */
    NULL,  /* sign */
    NULL,  /* verify */
    kem_get_nbits,
    NULL,  /* selftests */
    kem_compute_keygrip,
    NULL,  /* get_curve */
    NULL   /* get_curve_param */
  };
