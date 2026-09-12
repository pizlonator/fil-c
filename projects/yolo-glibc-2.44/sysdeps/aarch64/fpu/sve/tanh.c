/* Double-precision vector (SVE) tanh function

   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include "sv_math.h"

static const struct data
{
  double ln2_hi, ln2_lo;
  double c2, c4;
  double c0, c1, c3;
  double two_over_ln2, shift;
  uint64_t tiny_bound;
  double large_bound, fexpa_bound;
  uint64_t e2xm1_data[20];
} data = {
  /* Generated using Remez, in [-log(2)/128, log(2)/128].  */
  .c0 = 0x1p-1,
  .c1 = 0x1.55555555548f9p-3,
  .c2 = 0x1.5555555554c22p-5,
  .c3 = 0x1.111123aaa2fb2p-7,
  .c4 = 0x1.6c16d77d98e5bp-10,
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  .two_over_ln2 = 0x1.71547652b82fep+1,
  .shift = 0x1.800000000ffc0p+46,   /* 1.5*2^46+1023.  */
  .tiny_bound = 0x3e40000000000000, /* asuint64 (0x1p-27).  */
  .large_bound = 0x1.30fc1931f09cap+4, /* arctanh(1 - 2^-54).  */
  .fexpa_bound = 0x1.a56ef8ec924ccp-4,	  /* 19/64 * ln2/2.  */
  /* Table lookup of 2^(i/64) - 1, for values of i from 0..19.  */
  .e2xm1_data = {
    0x0000000000000000, 0x3f864d1f3bc03077, 0x3f966c34c5615d0f, 0x3fa0e8a30eb37901,
    0x3fa6ab0d9f3121ec, 0x3fac7d865a7a3440, 0x3fb1301d0125b50a, 0x3fb429aaea92ddfb,
    0x3fb72b83c7d517ae, 0x3fba35beb6fcb754, 0x3fbd4873168b9aa8, 0x3fc031dc431466b2,
    0x3fc1c3d373ab11c3, 0x3fc35a2b2f13e6e9, 0x3fc4f4efa8fef709, 0x3fc6942d3720185a,
    0x3fc837f0518db8a9, 0x3fc9e0459320b7fa, 0x3fcb8d39b9d54e55, 0x3fcd3ed9a72cffb7,
  },
};

/* An expm1 inspired, FEXPA based helper function that returns an
   accurate estimate for e^2x - 1. With no special case or support for
   negative inputs of x.  */
static inline svfloat64_t
e2xm1_inline (const svbool_t pg, svfloat64_t x, const struct data *d)
{
  svfloat64_t z = svmla_x (pg, sv_f64 (d->shift), x, d->two_over_ln2);
  svuint64_t u = svreinterpret_u64 (z);
  svfloat64_t n = svsub_x (pg, z, d->shift);

  /* r = x - n * ln2/2, r is in [-ln2/(2N), ln2/(2N)].  */
  svfloat64_t ln2 = svld1rq (svptrue_b64 (), &d->ln2_hi);
  svfloat64_t r = svadd_x (pg, x, x);
  r = svmls_lane (r, n, ln2, 0);
  r = svmls_lane (r, n, ln2, 1);

  /* y = exp(r) - 1 ~= r + C0 r^2 + C1 r^3 + C2 r^4 + C3 r^5 + C4 r^6.  */
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t c24 = svld1rq (svptrue_b64 (), &d->c2);

  svfloat64_t p;
  svfloat64_t c12 = svmla_lane (sv_f64 (d->c1), r, c24, 0);
  svfloat64_t c34 = svmla_lane (sv_f64 (d->c3), r, c24, 1);
  p = svmad_x (pg, c34, r2, c12);
  p = svmad_x (pg, p, r, sv_f64 (d->c0));
  p = svmad_x (pg, p, r2, r);

  svfloat64_t scale = svexpa (u);

  /* We want to construct e2xm1(x) = (scale - 1) + scale * poly.
     However, for values of scale close to 1, scale-1 causes large ULP errors
     due to cancellation.

     This can be circumvented by using a small lookup for scale-1
     when our input is below a certain bound, otherwise we can use FEXPA.  */
  svbool_t is_small = svaclt (pg, x, d->fexpa_bound);

  /* Index via the input of FEXPA, but we only care about the lower 5 bits.  */
  svuint64_t base_idx = svand_x (pg, u, 0x1f);

  /* Compute scale - 1 from FEXPA, and lookup values where this fails.  */
  svfloat64_t scalem1_estimate = svsub_x (pg, scale, sv_f64 (1.0));
  svuint64_t scalem1_lookup
      = svld1_gather_index (is_small, d->e2xm1_data, base_idx);

  /* Select the appropriate scale - 1 value based on x.  */
  svfloat64_t scalem1
      = svsel (is_small, svreinterpret_f64 (scalem1_lookup), scalem1_estimate);
  return svmla_x (pg, scalem1, scale, p);
}

/* SVE approximation for double-precision tanh(x), using a modified version of
   FEXPA expm1 to calculate e^2x - 1.
   The greatest observed error is 2.79 + 0.5 ULP:
   _ZGVsMxv_tanh (0x1.fff868eb3c223p-9) got 0x1.fff7be486cae6p-9
				       want 0x1.fff7be486cae9p-9.  */
svfloat64_t SV_NAME_D1 (tanh) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svbool_t large = svacge (pg, x, d->large_bound);

  /* We can use tanh(x) = (e^2x - 1) / (e^2x + 1) to approximate tanh.
  As an additional optimisation, we can ensure more accurate values of e^x
  by only using positive inputs. So we calculate tanh(|x|), and restore the
  sign of the input before returning.  */
  svfloat64_t ax = svabs_x (pg, x);
  svuint64_t sign_bit
      = sveor_x (pg, svreinterpret_u64 (x), svreinterpret_u64 (ax));

  svfloat64_t p = e2xm1_inline (pg, ax, d);
  svfloat64_t q = svadd_x (pg, p, 2);

  /* For sufficiently high inputs, the result of tanh(|x|) is 1 when correctly
     rounded, at this point we can return 1 directly, with sign correction.
     This will also act as a guard against our approximation overflowing.  */
  svfloat64_t y = svsel (large, sv_f64 (1.0), svdiv_x (pg, p, q));

  return svreinterpret_f64 (svorr_x (pg, sign_bit, svreinterpret_u64 (y)));
}
