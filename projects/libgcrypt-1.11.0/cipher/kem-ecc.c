/* kem-ecc.c - Key Encapsulation Mechanism with ECC
 * Copyright (C) 2024 g10 Code GmbH
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser general Public License as
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
 *
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "g10lib.h"
#include "cipher.h"

#include "kem-ecc.h"

#define ECC_PUBKEY_LEN_MAX 133
#define ECC_SECKEY_LEN_MAX 66

static const char *
algo_to_curve (int algo)
{
  switch (algo)
    {
    case GCRY_KEM_RAW_X25519:
    case GCRY_KEM_DHKEM25519:
      return "Curve25519";

    case GCRY_KEM_RAW_X448:
    case GCRY_KEM_DHKEM448:
      return "X448";

    case GCRY_KEM_RAW_BP256:
      return "brainpoolP256r1";

    case GCRY_KEM_RAW_BP384:
      return "brainpoolP384r1";

    case GCRY_KEM_RAW_BP512:
      return "brainpoolP512r1";

    case GCRY_KEM_RAW_P256R1:
      return "NIST P-256";

    case GCRY_KEM_RAW_P384R1:
      return "NIST P-384";

    case GCRY_KEM_RAW_P521R1:
      return "NIST P-521";

    default:
      return 0;
    }
}


static int
algo_to_seckey_len (int algo)
{
  switch (algo)
    {
    case GCRY_KEM_RAW_X25519:
    case GCRY_KEM_DHKEM25519:
      return 32;

    case GCRY_KEM_RAW_X448:
    case GCRY_KEM_DHKEM448:
      return 56;

    case GCRY_KEM_RAW_BP256:
      return 32;

    case GCRY_KEM_RAW_BP384:
      return 48;

    case GCRY_KEM_RAW_BP512:
      return 64;

    case GCRY_KEM_RAW_P256R1:
      return 32;

    case GCRY_KEM_RAW_P384R1:
      return 48;

    case GCRY_KEM_RAW_P521R1:
      return 66;

    default:
      return 0;
    }
}


static gpg_err_code_t
ecc_mul_point (int algo, unsigned char *result, size_t result_len,
               const unsigned char *scalar, size_t scalar_len,
               const unsigned char *point, size_t point_len)
{
  const char *curve = algo_to_curve (algo);

  return _gcry_ecc_curve_mul_point (curve, result, result_len,
                                    scalar, scalar_len, point, point_len);
}


gpg_err_code_t
_gcry_ecc_raw_keypair (int algo, void *pubkey, size_t pubkey_len,
                       void *seckey, size_t seckey_len)
{
  const char *curve = algo_to_curve (algo);

  return _gcry_ecc_curve_keypair (curve,
                                  pubkey, pubkey_len, seckey, seckey_len);
}

gpg_err_code_t
_gcry_ecc_raw_encap (int algo, const void *pubkey, size_t pubkey_len,
                     void *ciphertext, size_t ciphertext_len,
                     void *shared, size_t shared_len)
{
  gpg_err_code_t err;
  unsigned char seckey_ephemeral[ECC_SECKEY_LEN_MAX];
  void *pubkey_ephemeral = ciphertext;
  size_t seckey_len;

  if (ciphertext_len != pubkey_len)
    return GPG_ERR_INV_VALUE;

  seckey_len = algo_to_seckey_len (algo);
  err = _gcry_ecc_raw_keypair (algo, pubkey_ephemeral, pubkey_len,
                               seckey_ephemeral, seckey_len);
  if (err)
    return err;

  /* Do ECDH.  */
  return ecc_mul_point (algo, shared, shared_len, seckey_ephemeral, seckey_len,
                        pubkey, pubkey_len);
}

gpg_err_code_t
_gcry_ecc_raw_decap (int algo, const void *seckey, size_t seckey_len,
                     const void *ciphertext, size_t ciphertext_len,
                     void *shared, size_t shared_len)
{
  /* Do ECDH.  */
  return ecc_mul_point (algo, shared, shared_len, seckey, seckey_len,
                        ciphertext, ciphertext_len);
}


enum
  {
    DHKEM_X25519_HKDF_SHA256 = 0x20, /* Defined in RFC 9180.  */
    DHKEM_X448_HKDF_SHA512   = 0x21
  };

static gpg_err_code_t
ecc_dhkem_kdf (int kem_algo, size_t ecc_len,
               const unsigned char *ecdh, const unsigned char *ciphertext,
               const unsigned char *pubkey, void *shared)
{
  gpg_err_code_t err;
  unsigned char *p;
  unsigned char labeled_ikm[7+5+7+ECC_PUBKEY_LEN_MAX];
  int labeled_ikm_size;
  unsigned char labeled_info[2+7+5+13+2*ECC_PUBKEY_LEN_MAX];
  int labeled_info_size;
  gcry_kdf_hd_t hd;
  unsigned long param[1];
  int macalgo;
  int mac_len;

  if (kem_algo == DHKEM_X25519_HKDF_SHA256)
    macalgo = GCRY_MAC_HMAC_SHA256;
  else if (kem_algo == DHKEM_X448_HKDF_SHA512)
    macalgo = GCRY_MAC_HMAC_SHA512;
  else
    return GPG_ERR_UNKNOWN_ALGORITHM;

  mac_len = _gcry_mac_get_algo_maclen (macalgo);
  param[0] = mac_len;
  labeled_ikm_size = 7+5+7+ecc_len;
  labeled_info_size = 2+7+5+13+ecc_len*2;

  p = labeled_ikm;
  memcpy (p, "HPKE-v1", 7);
  p += 7;
  memcpy (p, "KEM", 3);
  p[3] = 0;
  p[4] = kem_algo;
  p += 5;
  memcpy (p, "eae_prk", 7);
  p += 7;
  memcpy (p, ecdh, ecc_len);

  p = labeled_info;
  /* length */
  p[0] = 0;
  p[1] = mac_len;
  p += 2;
  memcpy (p, "HPKE-v1", 7);
  p += 7;
  memcpy (p, "KEM", 3);
  p[3] = 0;
  p[4] = kem_algo;
  p += 5;
  memcpy (p, "shared_secret", 13);
  p += 13;
  /* kem_context */
  memcpy (p, ciphertext, ecc_len);
  p += ecc_len;
  memcpy (p, pubkey, ecc_len);
  p += ecc_len;

  err = _gcry_kdf_open (&hd, GCRY_KDF_HKDF, macalgo, param, 1,
                        labeled_ikm, labeled_ikm_size,
                        NULL, 0, NULL, 0, labeled_info, labeled_info_size);
  if (err)
    return err;

  err = _gcry_kdf_compute (hd, NULL);
  if (!err)
    err = _gcry_kdf_final (hd, mac_len, shared);
  _gcry_kdf_close (hd);
  return err;
}


gpg_err_code_t
_gcry_ecc_dhkem_encap (int algo, const void *pubkey, void *ciphertext,
                       void *shared)
{
  gpg_err_code_t err;
  unsigned char ecdh[ECC_PUBKEY_LEN_MAX];
  unsigned char seckey_ephemeral[ECC_SECKEY_LEN_MAX];
  void *pubkey_ephemeral = ciphertext;
  int curveid;
  int kem_algo;
  size_t ecc_len;

  if (algo == GCRY_KEM_DHKEM25519)
    {
      curveid = GCRY_ECC_CURVE25519;
      kem_algo = DHKEM_X25519_HKDF_SHA256;
    }
  else if (algo == GCRY_KEM_DHKEM448)
    {
      curveid = GCRY_ECC_CURVE448;
      kem_algo = DHKEM_X448_HKDF_SHA512;
    }
  else
    return GPG_ERR_UNKNOWN_ALGORITHM;

  ecc_len = _gcry_ecc_get_algo_keylen (curveid);

  err = _gcry_ecc_raw_keypair (algo, pubkey_ephemeral, ecc_len,
                               seckey_ephemeral, ecc_len);
  if (err)
    return err;

  /* Do ECDH.  */
  err = ecc_mul_point (algo, ecdh, ecc_len, seckey_ephemeral, ecc_len,
                       pubkey, ecc_len);
  if (err)
    return err;

  return ecc_dhkem_kdf (kem_algo, ecc_len, ecdh, ciphertext, pubkey, shared);
}

gpg_err_code_t
_gcry_ecc_dhkem_decap (int algo, const void *seckey, const void *ciphertext,
                       void *shared, const void *optional)
{
  gpg_err_code_t err;
  unsigned char ecdh[ECC_PUBKEY_LEN_MAX];
  unsigned char pubkey_computed[ECC_PUBKEY_LEN_MAX];
  const unsigned char *pubkey;
  int curveid;
  int kem_algo;
  size_t ecc_len;

  if (algo == GCRY_KEM_DHKEM25519)
    {
      curveid = GCRY_ECC_CURVE25519;
      kem_algo = DHKEM_X25519_HKDF_SHA256;
    }
  else if (algo == GCRY_KEM_DHKEM448)
    {
      curveid = GCRY_ECC_CURVE448;
      kem_algo = DHKEM_X448_HKDF_SHA512;
    }
  else
    return GPG_ERR_UNKNOWN_ALGORITHM;

  ecc_len = _gcry_ecc_get_algo_keylen (curveid);

  if (optional)
    pubkey = optional;
  else
    {
      err = ecc_mul_point (algo, pubkey_computed, ecc_len, seckey, ecc_len,
                           NULL, ecc_len);
      if (err)
        return err;

      pubkey = pubkey_computed;
    }

  /* Do ECDH.  */
  err = ecc_mul_point (algo, ecdh, ecc_len, seckey, ecc_len,
                       ciphertext, ecc_len);
  if (err)
    return err;

  return ecc_dhkem_kdf (kem_algo, ecc_len, ecdh, ciphertext, pubkey, shared);
}
