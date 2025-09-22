/* Test mpn_addaddmul_1msb0.

Copyright 2021 Free Software Foundation,
Inc.

This file is part of the GNU MP Library test suite.

The GNU MP Library test suite is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

The GNU MP Library test suite is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
the GNU MP Library test suite.  If not, see https://www.gnu.org/licenses/.  */

#include <stdio.h>
#include <stdlib.h>

#include "gmp-impl.h"
#include "tests.h"

#if !HAVE_NATIVE_mpn_addaddmul_1msb0
int main(int argc, char **argv) {
  return 77;  /* Test driver "SKIP" */
}
#else

static void
one_test (int i, mp_srcptr a, mp_srcptr b, mp_size_t n, mp_limb_t u, mp_limb_t v)
{
  mp_ptr r = refmpn_malloc_limbs (n + 1);
  mp_ptr ref = refmpn_malloc_limbs (n + 1);

  u &= ~GMP_NUMB_HIGHBIT;
  v &= ~GMP_NUMB_HIGHBIT;
  ref[n] = mpn_mul_1 (ref, a, n, u);
  ref[n] += mpn_addmul_1 (ref, b, n, v);
  r[n] = mpn_addaddmul_1msb0 (r, a, b, n, u, v);

  if (mpn_cmp (r, ref, n+1) != 0)
    {
      fprintf (stderr, "ERROR in test %d\n", i);
      fprintf (stderr, "Bad result from addaddmul_1msb0\n");
      gmp_fprintf (stderr, "op1=%Nx\n", a, n);
      gmp_fprintf (stderr, "op2=%Nx\n", b, n);
      gmp_fprintf (stderr, "u = %Mx, v = %Mx\n", u, v);
      gmp_fprintf (stderr, "res=%Nx\n", r, n + 1);
      gmp_fprintf (stderr, "ref=%Nx\n", ref, n + 1);

      abort();
    }
}

int main (int argc, char **argv)
{
  mpz_t op1, op2;
  int i;
  gmp_randstate_ptr rands;
  mpz_t bs;

  tests_start ();
  rands = RANDS;

  mpz_inits (bs, op1, op2, NULL);

  for (i = 0; i < 10000; i++)
    {
      unsigned long size_range;
      mp_size_t bit_size;
      mp_size_t limb_size;
      mp_limb_t u, v;

      mpz_urandomb (bs, rands, 32);
      size_range = mpz_get_ui (bs) % 10 + 2;
      mpz_urandomb (bs, rands, size_range);

      bit_size = mpz_get_ui (bs) + 10;
      mpz_rrandomb (op1, rands, bit_size);
      mpz_rrandomb (op2, rands, bit_size);

      mpz_rrandomb (bs, rands, GMP_NUMB_BITS - 1);
      u = mpz_getlimbn (bs, 0);

      mpz_rrandomb (bs, rands, GMP_NUMB_BITS - 1);
      v = mpz_getlimbn (bs, 0);

      limb_size = mpz_size (op1);
      one_test (i, mpz_limbs_read (op1), mpz_limbs_read(op2), limb_size, u, v);
    }
  mpz_clears (bs, op1, op2, NULL);
  return 0;
}
#endif
