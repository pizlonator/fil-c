/* mpih-const-time.c  -  Constant-time MPI helper functions
 *      Copyright (C) 2020  g10 Code GmbH
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
#include "mpi-internal.h"
#include "g10lib.h"
#include "const-time.h"
#include "longlong.h"

#define A_LIMB_1 ((mpi_limb_t)1)


/*
 * Return 1 if X > Y and otherwise return 0.
 */
static inline mpi_limb_t
mpih_ct_limb_greater_than (mpi_limb_t x, mpi_limb_t y)
{
  mpi_limb_t diff_hi, diff_lo;
  sub_ddmmss (diff_hi, diff_lo, 0, y, 0, x);
  (void)diff_lo;
  return diff_hi >> (BITS_PER_MPI_LIMB - 1);
}


/*
 * Return 1 if X < Y and otherwise return 0.
 */
static inline mpi_limb_t
mpih_ct_limb_less_than (mpi_limb_t x, mpi_limb_t y)
{
  return mpih_ct_limb_greater_than (y, x);
}


/*
 *  W = U when OP_ENABLED=1
 *  otherwise, W keeps old value
 */
void
_gcry_mpih_set_cond (mpi_ptr_t wp, mpi_ptr_t up, mpi_size_t usize,
                     unsigned long op_enable)
{
  /* Note: dual mask with AND/OR used for EM leakage mitigation */
  mpi_limb_t mask1 = ct_limb_gen_mask(op_enable);
  mpi_limb_t mask2 = ct_limb_gen_inv_mask(op_enable);
  mpi_size_t i;

  for (i = 0; i < usize; i++)
    {
      wp[i] = (wp[i] & mask2) | (up[i] & mask1);
    }
}


/*
 *  W = U + V when OP_ENABLED=1
 *  otherwise, W = U
 */
mpi_limb_t
_gcry_mpih_add_n_cond (mpi_ptr_t wp, mpi_ptr_t up, mpi_ptr_t vp,
                       mpi_size_t usize, unsigned long op_enable)
{
  /* Note: dual mask with AND/OR used for EM leakage mitigation */
  mpi_limb_t mask1 = ct_limb_gen_mask(op_enable);
  mpi_limb_t mask2 = ct_limb_gen_inv_mask(op_enable);
  mpi_size_t i;
  mpi_limb_t cy;

  cy = 0;
  for (i = 0; i < usize; i++)
    {
      mpi_limb_t u = up[i];
      mpi_limb_t x = u + vp[i];
      mpi_limb_t cy1 = mpih_ct_limb_less_than(x, u);
      mpi_limb_t cy2;

      x = x + cy;
      cy2 = mpih_ct_limb_less_than(x, cy);
      cy = cy1 | cy2;
      wp[i] = (u & mask2) | (x & mask1);
    }

  return cy & mask1;
}


/*
 *  W = U - V when OP_ENABLED=1
 *  otherwise, W = U
 */
mpi_limb_t
_gcry_mpih_sub_n_cond (mpi_ptr_t wp, mpi_ptr_t up, mpi_ptr_t vp,
                       mpi_size_t usize, unsigned long op_enable)
{
  /* Note: dual mask with AND/OR used for EM leakage mitigation */
  mpi_limb_t mask1 = ct_limb_gen_mask(op_enable);
  mpi_limb_t mask2 = ct_limb_gen_inv_mask(op_enable);
  mpi_size_t i;
  mpi_limb_t cy;

  cy = 0;
  for (i = 0; i < usize; i++)
    {
      mpi_limb_t u = up[i];
      mpi_limb_t x = u - vp[i];
      mpi_limb_t cy1 = mpih_ct_limb_greater_than(x, u);
      mpi_limb_t cy2;

      cy2 = mpih_ct_limb_less_than(x, cy);
      x = x - cy;
      cy = cy1 | cy2;
      wp[i] = (u & mask2) | (x & mask1);
    }

  return cy & mask1;
}


/*
 *  Swap value of U and V when OP_ENABLED=1
 *  otherwise, no change
 */
void
_gcry_mpih_swap_cond (mpi_ptr_t up, mpi_ptr_t vp, mpi_size_t usize,
                      unsigned long op_enable)
{
  /* Note: dual mask with AND/OR used for EM leakage mitigation */
  mpi_limb_t mask1 = ct_limb_gen_mask(op_enable);
  mpi_limb_t mask2 = ct_limb_gen_inv_mask(op_enable);
  mpi_size_t i;

  for (i = 0; i < usize; i++)
    {
      mpi_limb_t u = up[i];
      mpi_limb_t v = vp[i];
      up[i] = (u & mask2) | (v & mask1);
      vp[i] = (u & mask1) | (v & mask2);
    }
}


/*
 *  W = -U when OP_ENABLED=1
 *  otherwise, W = U
 */
void
_gcry_mpih_abs_cond (mpi_ptr_t wp, mpi_ptr_t up, mpi_size_t usize,
                     unsigned long op_enable)
{
  /* Note: dual mask with AND/OR used for EM leakage mitigation */
  mpi_limb_t mask1 = ct_limb_gen_mask(op_enable);
  mpi_limb_t mask2 = ct_limb_gen_inv_mask(op_enable);
  mpi_limb_t cy = op_enable;
  mpi_size_t i;

  for (i = 0; i < usize; i++)
    {
      mpi_limb_t u = up[i];
      mpi_limb_t x = ~u + cy;

      cy = mpih_ct_limb_less_than(x, ~u);
      wp[i] = (u & mask2) | (x & mask1);
    }
}


/*
 * Allocating memory for W,
 * compute W = V % U, then return W
 */
mpi_ptr_t
_gcry_mpih_mod (mpi_ptr_t vp, mpi_size_t vsize,
                mpi_ptr_t up, mpi_size_t usize)
{
  int secure;
  mpi_ptr_t rp;
  mpi_size_t i;

  secure = _gcry_is_secure (vp);
  rp = mpi_alloc_limb_space (usize, secure);
  MPN_ZERO (rp, usize);

  for (i = 0; i < vsize * BITS_PER_MPI_LIMB; i++)
    {
      unsigned int j = vsize * BITS_PER_MPI_LIMB - 1 - i;
      unsigned int limbno = j / BITS_PER_MPI_LIMB;
      unsigned int bitno = j % BITS_PER_MPI_LIMB;
      mpi_limb_t limb = vp[limbno];
      unsigned int the_bit = (limb >> bitno) & 1;
      mpi_limb_t underflow;
      mpi_limb_t overflow;

      overflow = _gcry_mpih_lshift (rp, rp, usize, 1);
      rp[0] |= the_bit;

      underflow = _gcry_mpih_sub_n (rp, rp, up, usize);
      mpih_add_n_cond (rp, rp, up, usize, overflow ^ underflow);
    }

  return rp;
}

int
_gcry_mpih_cmp_ui (mpi_ptr_t up, mpi_size_t usize, unsigned long v)
{
  int is_all_zero = 1;
  mpi_size_t i;

  for (i = 1; i < usize; i++)
    is_all_zero &= mpih_limb_is_zero (up[i]);

  if (is_all_zero)
    {
      if (up[0] < v)
        return -1;
      else if (up[0] > v)
        return 1;
      else
        return 0;
    }
  return 1;
}
