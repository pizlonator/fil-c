/* mpn_mul_basecase for IBM z13 and later -- Internal routine to multiply two
   natural numbers of length m and n.

   THIS IS AN INTERNAL FUNCTION WITH A MUTABLE INTERFACE.  IT IS ONLY
   SAFE TO REACH THIS FUNCTION THROUGH DOCUMENTED INTERFACES.

Copyright 2021 Free Software Foundation, Inc.

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

#include <stdlib.h>

#include "gmp-impl.h"

/* Note: we explicitly inline all mul and addmul routines here to reduce the
 * number of branches in prologues of unrolled functions. That comes at the
   cost of duplicating common loop bodies in object code. */
#define DO_INLINE

/*
 * tweak loop conditions in addmul subroutines to enable use of
 * branch-relative-on-count (BRCTG) instructions, which currently results in
 * better performance.
 */
#define BRCTG

#include "s390_64/z13/common-vec.h"

#define OPERATION_mul_1
#include "s390_64/z13/addmul_1.c"
#undef OPERATION_mul_1

#define OPERATION_addmul_1
#include "s390_64/z13/addmul_1.c"
#undef OPERATION_addmul_1

#define OPERATION_mul_2
#include "s390_64/z13/aormul_2.c"
#undef OPERATION_mul_2

#define OPERATION_addmul_2
#include "s390_64/z13/aormul_2.c"
#undef OPERATION_addmul_2

void
mpn_mul_basecase (mp_ptr rp, mp_srcptr up, mp_size_t un, mp_srcptr vp,
                  mp_size_t vn)
{
  ASSERT (un >= vn);
  ASSERT (vn >= 1);
  ASSERT (!MPN_OVERLAP_P (rp, un + vn, up, un));
  ASSERT (!MPN_OVERLAP_P (rp, un + vn, vp, vn));

  /* The implementations of (add)mul_1/2 are 4x-unrolled. Pull out the branch
   * for un%4 and inline specific variants. */

#define BRANCH_FOR_MOD(N)                                                     \
  do                                                                          \
    {                                                                         \
      if (vn >= 2)                                                            \
        {                                                                     \
          rp[un + 1] = inline_mul_2 (rp, up, un, vp);                         \
          rp += 2, vp += 2, vn -= 2;                                          \
        }                                                                     \
      else                                                                    \
        {                                                                     \
          rp[un] = inline_mul_1 (rp, up, un, vp[0]);                          \
          return;                                                             \
        }                                                                     \
                                                                              \
      while (vn >= 2)                                                         \
        {                                                                     \
          rp[un + 2 - 1] = inline_addmul_2 (rp, up, un, vp);                  \
          rp += 2, vp += 2, vn -= 2;                                          \
        }                                                                     \
                                                                              \
      while (vn >= 1)                                                         \
        {                                                                     \
          rp[un] = inline_addmul_1 (rp, up, un, vp[0]);                       \
          rp += 1, vp += 1, vn -= 1;                                          \
        }                                                                     \
    }                                                                         \
  while (0);

  switch (((size_t)un) % 4)
    {
    case 0:
      BRANCH_FOR_MOD (0);
      break;
    case 1:
      BRANCH_FOR_MOD (1);
      break;
    case 2:
      BRANCH_FOR_MOD (2);
      break;
    case 3:
      BRANCH_FOR_MOD (3);
      break;
    }
}
