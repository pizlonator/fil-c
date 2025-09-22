/* mpn_sqr_basecase -- Internal routine to square a natural number of length n.
   This is a place-holder for z13 to suppress the use of the plain z/arch code.
   FIXME: This should really be written in assembly with outer-loop early exit.

   THIS IS AN INTERNAL FUNCTION WITH A MUTABLE INTERFACE.  IT IS ONLY
   SAFE TO REACH THIS FUNCTION THROUGH DOCUMENTED INTERFACES.


Copyright 1991-1994, 1996, 1997, 2000-2005, 2008, 2010, 2011, 2017, 2023 Free
Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of either:

  * the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your
    option) any later version.

or

  * the GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any
    later version.

or both in parallel, as here.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received copies of the GNU General Public License and the
GNU Lesser General Public License along with the GNU MP Library.  If not,
see https://www.gnu.org/licenses/.  */

#include "gmp-impl.h"
#include "longlong.h"

void
mpn_sqr_basecase (mp_ptr rp, mp_srcptr up, mp_size_t un)
{
  mp_limb_t u0;
  mp_limb_t cin;

  u0 = up[0];
  umul_ppmm (cin, rp[0], u0, u0);
  ++rp;

  if (--un) {
    u0 = u0 << 1;
    up += 1;

    rp[un] = mpn_mul_1c (rp, up, un, u0, cin);

    for (;;) {
      mp_limb_t ci, x0, c0, hi, lo, x1, c1;

      u0 = up[0];
      ci = -(up[-1] >> (GMP_NUMB_BITS-1)) & u0; // correction term
      x0 = rp[1] + ci;
      c0 = x0 < ci;
      hi, lo;

      umul_ppmm (hi, lo, u0, u0);
      x1 = x0 + lo;
      c1 = x1 < lo;
      cin = hi + c0 + c1;
      rp[1] = x1;
      rp += 2;

      if (--un == 0) break;
      u0 = (up[-1] >> (GMP_NUMB_BITS-1)) + (u0 << 1);
      up += 1;

      rp[un] = mpn_addmul_1c (rp, up, un, u0, cin);
    }
  }

  rp[0] = cin;
}
