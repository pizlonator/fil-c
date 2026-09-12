/* Configuration for libmvec routines.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _VECMATH_CONFIG_H
#define _VECMATH_CONFIG_H

#include <math_private.h>

/* Return ptr but hide its value from the compiler so accesses through it
   cannot be optimized based on the contents.  */
#define ptr_barrier(ptr)                                                      \
  ({                                                                          \
    __typeof (ptr) __ptr = (ptr);                                             \
    __asm("" : "+r"(__ptr));                                                  \
    __ptr;                                                                    \
  })

#define V_LOG_POLY_ORDER 6
#define V_LOG_TABLE_BITS 7
extern const struct v_log_data
{
  /* Shared data for vector log and log-derived routines (e.g. asinh).  */
  double poly[V_LOG_POLY_ORDER - 1];
  double ln2;
  struct
  {
    double invc, logc;
  } table[1 << V_LOG_TABLE_BITS];
} __v_log_data attribute_hidden;

#define V_EXP_TAIL_TABLE_BITS 8
extern const uint64_t __v_exp_tail_data[1 << V_EXP_TAIL_TABLE_BITS] attribute_hidden;
#define V_EXP_TABLE_BITS 7
extern const uint64_t __v_exp_data[1 << V_EXP_TABLE_BITS] attribute_hidden;

#define V_LOG2_TABLE_BITS 7
extern const struct v_log2_data
{
  double poly[5];
  double invln2;
  struct
  {
    double invc, log2c;
  } table[1 << V_LOG2_TABLE_BITS];
} __v_log2_data attribute_hidden;

#define V_LOG10_TABLE_BITS 7
extern const struct v_log10_data
{
  double poly[5];
  double invln10, log10_2;
  struct
  {
    double invc, log10c;
  } table[1 << V_LOG10_TABLE_BITS];
} __v_log10_data attribute_hidden;

extern const struct v_erff_data
{
  struct
  {
    float erf, scale;
  } tab[513];
} __v_erff_data attribute_hidden;

extern const struct v_erf_data
{
  struct
  {
    double erf, scale;
  } tab[769];
} __v_erf_data attribute_hidden;

extern const struct v_erfc_data
{
  struct
  {
    double erfc, scale;
  } tab[3488];
} __v_erfc_data attribute_hidden;

extern const struct v_erfcf_data
{
  struct
  {
    float erfc, scale;
  } tab[645];
} __v_erfcf_data attribute_hidden;

/* Some data for AdvSIMD and SVE pow's internal exp and log.  */
#define V_POW_EXP_TABLE_BITS 8
extern const struct v_pow_exp_data
{
  double poly[3];
  double n_over_ln2, ln2_over_n_hi, ln2_over_n_lo, shift;
  uint64_t sbits[1 << V_POW_EXP_TABLE_BITS];
} __v_pow_exp_data attribute_hidden;

#define V_POW_LOG_TABLE_BITS 7
extern const struct v_pow_log_data
{
  double poly[7]; /* First coefficient is 1.  */
  double ln2_hi, ln2_lo;
  double invc[1 << V_POW_LOG_TABLE_BITS];
  double logc[1 << V_POW_LOG_TABLE_BITS];
  double logctail[1 << V_POW_LOG_TABLE_BITS];
} __v_pow_log_data attribute_hidden;

/* Some data for SVE powf's internal exp and log.  */
#define V_POWF_EXP2_TABLE_BITS 5
#define V_POWF_EXP2_N (1 << V_POWF_EXP2_TABLE_BITS)
#define V_POWF_LOG2_TABLE_BITS 5
#define V_POWF_LOG2_N (1 << V_POWF_LOG2_TABLE_BITS)
extern const struct v_powf_data
{
  double invc[V_POWF_LOG2_N];
  double logc[V_POWF_LOG2_N];
  uint64_t scale[V_POWF_EXP2_N];
} __v_powf_data attribute_hidden;

#endif
