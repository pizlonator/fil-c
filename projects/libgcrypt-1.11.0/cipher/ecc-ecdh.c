/* ecc-ecdh.c  -  Elliptic Curve Diffie-Hellman key agreement
 * Copyright (C) 2019 g10 Code GmbH
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "g10lib.h"
#include "mpi.h"
#include "cipher.h"
#include "context.h"
#include "ec-context.h"
#include "ecc-common.h"

#define ECC_CURVE25519_BYTES 32
#define ECC_CURVE448_BYTES   56

static gpg_err_code_t
prepare_ec (mpi_ec_t *r_ec, const char *name)
{
  int flags = 0;

  if (!strcmp (name, "Curve25519"))
    flags = PUBKEY_FLAG_DJB_TWEAK;

  return _gcry_mpi_ec_internal_new (r_ec, &flags, "ecc_mul_point", NULL, name);
}

unsigned int
_gcry_ecc_get_algo_keylen (int curveid)
{
  unsigned int len = 0;

  if (curveid == GCRY_ECC_CURVE25519)
    len = ECC_CURVE25519_BYTES;
  else if (curveid == GCRY_ECC_CURVE448)
    len = ECC_CURVE448_BYTES;

  return len;
}

/* For Curve25519 and X448, we need to mask the bits and enable the MSB.  */
static void
ecc_tweak_bits (unsigned char *seckey, size_t seckey_len)
{
  if (seckey_len == 32)
    {
      seckey[0] &= 0xf8;
      seckey[31] &= 0x7f;
      seckey[31] |= 0x40;
    }
  else
    {
      seckey[0] &= 0xfc;
      seckey[55] |= 0x80;
    }
}

gpg_err_code_t
_gcry_ecc_curve_keypair (const char *curve,
                         unsigned char *pubkey, size_t pubkey_len,
                         unsigned char *seckey, size_t seckey_len)
{
  gpg_err_code_t err;
  unsigned int nbits;
  unsigned int nbytes;
  gcry_mpi_t mpi_k = NULL;
  mpi_ec_t ec = NULL;
  mpi_point_struct Q = { NULL, NULL, NULL };
  gcry_mpi_t x;
  unsigned int len;
  unsigned char *buf;

  err = prepare_ec (&ec, curve);
  if (err)
    return err;

  nbits = ec->nbits;
  nbytes = (nbits + 7)/8;

  if (seckey_len != nbytes)
    return GPG_ERR_INV_ARG;

  if (ec->model == MPI_EC_WEIERSTRASS)
    {
      if (pubkey_len != 1 + 2*nbytes)
        return GPG_ERR_INV_ARG;

      do
        {
          mpi_free (mpi_k);
          mpi_k = mpi_new (nbytes*8);
          _gcry_randomize (seckey, nbytes, GCRY_STRONG_RANDOM);
          _gcry_mpi_set_buffer (mpi_k, seckey, nbytes, 0);
        }
      while (mpi_cmp (mpi_k, ec->n) >= 0);
    }
  else if (ec->model == MPI_EC_MONTGOMERY)
    {
      if (pubkey_len != nbytes)
        return GPG_ERR_INV_ARG;

      _gcry_randomize (seckey, nbytes, GCRY_STRONG_RANDOM);
      /* Existing ECC applications with libgcrypt (like gpg-agent in
         GnuPG) assumes that scalar is tweaked at key generation time.
         For the possible use case where generated key with this routine
         may be used with those, we put compatibile behavior here.  */
      ecc_tweak_bits (seckey, nbytes);
      mpi_k = _gcry_mpi_set_opaque_copy (NULL, seckey, nbytes*8);
    }
  else
    return GPG_ERR_UNKNOWN_CURVE;

  x = mpi_new (nbits);
  point_init (&Q);

  _gcry_mpi_ec_mul_point (&Q, mpi_k, ec->G, ec);

  if (ec->model == MPI_EC_WEIERSTRASS)
    {
      gcry_mpi_t y = mpi_new (nbits);
      gcry_mpi_t negative = mpi_new (nbits);

      _gcry_mpi_ec_get_affine (x, y, &Q, ec);
      /* For the backward compatibility, we check if it's a
         "compliant key".  */

      mpi_sub (negative, ec->p, y);
      if (mpi_cmp (negative, y) < 0)   /* p - y < p */
        {
          mpi_free (y);
          y = negative;
          mpi_sub (mpi_k, ec->n, mpi_k);
          buf = _gcry_mpi_get_buffer (mpi_k, 0, &len, NULL);
          memset (seckey, 0, nbytes - len);
          memcpy (seckey + nbytes - len, buf, len);
        }
      else /* p - y >= p */
        mpi_free (negative);

      buf = _gcry_ecc_ec2os_buf (x, y, ec->p, &len);
      if (!buf)
        {
          err = gpg_err_code_from_syserror ();
          mpi_free (y);
        }
      else
        {
          if (len != 1 + 2*nbytes)
            {
              err = GPG_ERR_INV_ARG;
              mpi_free (y);
            }
          else
            {
              /* (x,y) in SEC1 point encoding.  */
              memcpy (pubkey, buf, len);
              xfree (buf);
              mpi_free (y);
            }
        }
    }
  else /* MPI_EC_MONTGOMERY */
    {
      _gcry_mpi_ec_get_affine (x, NULL, &Q, ec);

      buf = _gcry_mpi_get_buffer (x, nbytes, &len, NULL);
      if (!buf)
        err = gpg_err_code_from_syserror ();
      else
        {
          memcpy (pubkey, buf, nbytes);
          xfree (buf);
        }
    }

  mpi_free (x);
  point_free (&Q);
  mpi_free (mpi_k);
  _gcry_mpi_ec_free (ec);
  return err;
}

gpg_err_code_t
_gcry_ecc_curve_mul_point (const char *curve,
                           unsigned char *result, size_t result_len,
                           const unsigned char *scalar, size_t scalar_len,
                           const unsigned char *point, size_t point_len)
{
  unsigned int nbits;
  unsigned int nbytes;
  gpg_err_code_t err;
  gcry_mpi_t mpi_k = NULL;
  mpi_ec_t ec = NULL;
  mpi_point_struct Q = { NULL, NULL, NULL };
  gcry_mpi_t x = NULL;
  unsigned int len;
  unsigned char *buf;

  err = prepare_ec (&ec, curve);
  if (err)
    return err;

  nbits = ec->nbits;
  nbytes = (nbits + 7)/8;

  if (ec->model == MPI_EC_WEIERSTRASS)
    {
      if (scalar_len != nbytes
          || result_len != 1 + 2*nbytes
          || point_len != 1 + 2*nbytes)
        {
          err = GPG_ERR_INV_ARG;
          goto leave;
        }

      mpi_k = mpi_new (nbytes*8);
      _gcry_mpi_set_buffer (mpi_k, scalar, nbytes, 0);
    }
  else if (ec->model == MPI_EC_MONTGOMERY)
    {
      if (scalar_len != nbytes
          || result_len != nbytes
          || point_len != nbytes)
        {
          err = GPG_ERR_INV_ARG;
          goto leave;
        }

      mpi_k = _gcry_mpi_set_opaque_copy (NULL, scalar, nbytes*8);
    }
  else
    {
      err = GPG_ERR_UNKNOWN_CURVE;
      goto leave;
    }

  point_init (&Q);

  if (point)
    {
      gcry_mpi_t mpi_u = _gcry_mpi_set_opaque_copy (NULL, point, point_len*8);
      mpi_point_struct P;

      point_init (&P);
      if (ec->model == MPI_EC_WEIERSTRASS)
        err = _gcry_ecc_sec_decodepoint (mpi_u, ec, &P);
      else /* MPI_EC_MONTGOMERY */
        err = _gcry_ecc_mont_decodepoint (mpi_u, ec, &P);
      mpi_free (mpi_u);
      if (err)
        goto leave;
      _gcry_mpi_ec_mul_point (&Q, mpi_k, &P, ec);
      point_free (&P);
    }
  else
    _gcry_mpi_ec_mul_point (&Q, mpi_k, ec->G, ec);

  x = mpi_new (nbits);
  if (ec->model == MPI_EC_WEIERSTRASS)
    {
      gcry_mpi_t y = mpi_new (nbits);

      _gcry_mpi_ec_get_affine (x, y, &Q, ec);

      buf = _gcry_ecc_ec2os_buf (x, y, ec->p, &len);
      if (!buf)
        {
          err = gpg_err_code_from_syserror ();
          mpi_free (y);
        }
      else
        {
          if (len != 1 + 2*nbytes)
            {
              err = GPG_ERR_INV_ARG;
              mpi_free (y);
            }
          else
            {
              /* (x,y) in SEC1 point encoding.  */
              memcpy (result, buf, len);
              xfree (buf);
              mpi_free (y);
            }
        }
    }
  else                          /* MPI_EC_MONTGOMERY */
    {
      _gcry_mpi_ec_get_affine (x, NULL, &Q, ec);
      buf = _gcry_mpi_get_buffer (x, nbytes, &len, NULL);
      if (!buf)
        err = gpg_err_code_from_syserror ();
      else
        {
          if (len != nbytes)
            err = GPG_ERR_INV_ARG;
          else
            {
              /* x in little endian.  */
              memcpy (result, buf, nbytes);
              xfree (buf);
            }
        }
    }
  mpi_free (x);

 leave:
  point_free (&Q);
  mpi_free (mpi_k);
  _gcry_mpi_ec_free (ec);
  return err;
}

gpg_err_code_t
_gcry_ecc_mul_point (int curveid, unsigned char *result,
                     const unsigned char *scalar, const unsigned char *point)
{
  const char *curve;
  size_t pubkey_len, seckey_len;

  if (curveid == GCRY_ECC_CURVE25519)
    {
      curve = "Curve25519";
      pubkey_len = seckey_len = 32;
    }
  else if (curveid == GCRY_ECC_CURVE448)
    {
      curve = "X448";
      pubkey_len = seckey_len = 56;
    }
  else
    return gpg_error (GPG_ERR_UNKNOWN_CURVE);

  return _gcry_ecc_curve_mul_point (curve, result, pubkey_len,
                                    scalar, seckey_len,
                                    point, pubkey_len);
}
