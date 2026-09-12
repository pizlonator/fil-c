/* Single-precision vector (Advanced SIMD) hypot function

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

#include "v_math.h"

static const struct data
{
  uint32x4_t tiny_bound;
  uint16x8_t thres;
} data = {
  .tiny_bound = V4 (0x0C800000), /* asuint (0x1p-102).  */
  .thres = V8 (0x7300), /* asuint (inf) - tiny_bound.  */
};

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, float32x4_t sqsum,
	      uint16x4_t special)
{
  return v_call2_f32 (hypotf, x, y, vsqrtq_f32 (sqsum), vmovl_u16 (special));
}

/* Vector implementation of single-precision hypot.
   Maximum error observed is 1.21 ULP:
   _ZGVnN4vv_hypotf (0x1.6a419cp-13, 0x1.82a852p-22) got 0x1.6a41d2p-13
						    want 0x1.6a41dp-13.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F2 (hypot) (float32x4_t x, float32x4_t y)
{
  const struct data *d = ptr_barrier (&data);

  float32x4_t sqsum = vfmaq_f32 (vmulq_f32 (x, x), y, y);

  uint16x4_t special = vcge_u16 (
      vsubhn_u32 (vreinterpretq_u32_f32 (sqsum), d->tiny_bound),
      vget_low_u16 (d->thres));

  if (__glibc_unlikely (v_any_u16h (special)))
    return special_case (x, y, sqsum, special);

  return vsqrtq_f32 (sqsum);
}
libmvec_hidden_def (V_NAME_F2 (hypot))
HALF_WIDTH_ALIAS_F2(hypot)
