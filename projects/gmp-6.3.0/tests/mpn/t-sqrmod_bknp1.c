/* Test for mulmod_bknp1 function.

   Contributed to the GNU project by Marco Bodrato.

Copyright 2009, 2020-2022 Free Software Foundation, Inc.

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


#include <stdlib.h>
#include <stdio.h>

#include "gmp-impl.h"
#include "tests.h"


#if MOD_BKNP1_USE11
#define USE11 11,
#else
#define USE11
#endif

#if GMP_NUMB_BITS % 32 == 0
#define MAX_K 17
#define SUPPORTED_K {3, 5, 7, 13, USE11 MAX_K}
#else
#if GMP_NUMB_BITS % 16 == 0
#define MAX_K 13
#define SUPPORTED_K {3, 5, 7, USE11 MAX_K}
#else
#if GMP_NUMB_BITS % 8 == 0
#define MAX_K 7
#define SUPPORTED_K {3, USE11 MAX_K}
#else
#define SUPPORTED_K {USE11} /* Supported ? */
#endif /* GMP_NUMB_BITS % 8 == 0 */
#endif /* GMP_NUMB_BITS % 16 == 0 */
#endif /* GMP_NUMB_BITS % 32 == 0 */

#if MOD_BKNP1_ONLY3
#undef SUPPORTED_K
#undef MAX_K
#define MAX_K 3
#define SUPPORTED_K {3}
#endif

/* Sizes are up to MAX_K * 2^SIZE_LOG limbs */
#ifndef SIZE_LOG
#define SIZE_LOG 7
#endif

#ifndef COUNT
#define COUNT 5000
#endif

#define MAX_N (MAX_K << SIZE_LOG)
#define MIN_N 1

/*
  Reference function for multiplication modulo B^{k*rn}+1.
*/

static void
ref_sqrmod_bnp1 (mp_ptr rp, mp_srcptr ap, mp_size_t rn)
{
  mp_limb_t cy;

  mpn_sqr (rp, ap, rn + 1);
  cy = rp[2 * rn];
  MPN_INCR_U (rp, 2 * rn + 1, rp[2 * rn]);
  cy = rp[2 * rn] - cy + mpn_sub_n (rp, rp, rp + rn, rn);
  rp[rn] = 0;
  MPN_INCR_U (rp, rn + 1, cy);
}

/*
  Compare the result of the mpn_mulmod_bnp1 function in the library
  with the reference function above.
*/
unsigned supported_k[] = SUPPORTED_K;

int
main (int argc, char **argv)
{
  mp_ptr ap, refp, pp, scratch;
  int count = COUNT;
  int test;
  gmp_randstate_ptr rands;
  TMP_DECL;
  TMP_MARK;

  TESTS_REPS (count, argv, argc);

  tests_start ();
  rands = RANDS;

  ap = TMP_ALLOC_LIMBS (MAX_N + 1);
  refp = TMP_ALLOC_LIMBS (MAX_N * 2 + 2);
  pp = 1 + TMP_ALLOC_LIMBS (MAX_N + 3);
  scratch
    = 1 + TMP_ALLOC_LIMBS (mpn_mulmod_bknp1_itch (MAX_N) + 2);

  for (test = 0; test < count; test++)
    {
      unsigned size_min;
      unsigned size_range;
      unsigned k;
      mp_size_t rn, n;
      mp_size_t itch;
      mp_limb_t p_before, p_after, s_before, s_after;

      for (size_min = 1; (1L << size_min) < MIN_N; size_min++)
	;

      /* We generate rn in the MIN_N <= n <= (1 << size_range). */
      size_range = size_min
	+ gmp_urandomm_ui (rands, SIZE_LOG + 1 - size_min);

      k = supported_k[test % numberof (supported_k)];
      if (test < numberof (supported_k))
	{
	  n = 1;
	  rn = k;
	  ap [rn] = 0;
	  mp_limb_t x = GMP_NUMB_MAX / k + 1;
	  ap [0] = x;
	  for (int i = 1; i < k; i += 2)
	    {
	      ap [i] = - x;
	      ap [i + 1] = x - 1;
	    }
	}
      else
	{
	  n = MIN_N
	    + gmp_urandomm_ui (rands, (1L << size_range) + 1 - MIN_N);
	  rn = k * n;
	  if ((GMP_NUMB_MAX % k != 0) && (rn % 3 == 0))
	    n = rn / (k = 3);

	  mpn_random2 (ap, rn + 1);

	  ap [rn] &= 1;
	}

      mpn_random2 (pp-1, rn + 3);
      p_before = pp[-1];
      p_after = pp[rn + 1];

      itch = mpn_sqrmod_bknp1_itch (rn);
      ASSERT_ALWAYS (itch <= mpn_mulmod_bknp1_itch (MAX_N));
      mpn_random2 (scratch - 1, itch + 2);
      s_before = scratch[-1];
      s_after = scratch[itch];

      mpn_sqrmod_bknp1 (  pp, ap, n, k, scratch);
      ref_sqrmod_bnp1 (refp, ap, rn);
      if (pp[-1] != p_before || pp[rn + 1] != p_after
	  || scratch[-1] != s_before || scratch[itch] != s_after
	  || mpn_cmp (refp, pp, rn + 1) != 0)
	{
	  printf ("ERROR in test %d(sqr), rn = %d, n = %d, k = %d\n",
		  test, (int) rn, (int) n, (int) k);
	  if (pp[-1] != p_before)
	    {
	      printf ("before pp:"); mpn_dump (pp - 1, 1);
	      printf ("keep:   "); mpn_dump (&p_before, 1);
	    }
	  if (pp[rn + 1] != p_after)
	    {
	      printf ("after pp:"); mpn_dump (pp + rn + 1, 1);
	      printf ("keep:   "); mpn_dump (&p_after, 1);
	    }
	  if (scratch[-1] != s_before)
	    {
	      printf ("before scratch:"); mpn_dump (scratch - 1, 1);
	      printf ("keep:   "); mpn_dump (&s_before, 1);
	    }
	  if (scratch[itch] != s_after)
	    {
	      printf ("after scratch:"); mpn_dump (scratch + itch, 1);
	      printf ("keep:   "); mpn_dump (&s_after, 1);
	    }
	  mpn_dump (ap, rn + 1);
	  mpn_dump (pp, rn + 1);
	  mpn_dump (refp, rn + 1);

	  abort();
	}

      mpn_random2 (pp-1, rn + 3);
      p_before = pp[-1];
      p_after = pp[rn + 1];

      itch = mpn_mulmod_bknp1_itch (rn);
      ASSERT_ALWAYS (itch <= mpn_mulmod_bknp1_itch (MAX_N));
      mpn_random2 (scratch - 1, itch + 2);
      s_before = scratch[-1];
      s_after = scratch[itch];

      mpn_mulmod_bknp1 (  pp, ap, ap, n, k, scratch);
      if (pp[-1] != p_before || pp[rn + 1] != p_after
	  || scratch[-1] != s_before || scratch[itch] != s_after
	  || mpn_cmp (refp, pp, rn + 1) != 0)
	{
	  printf ("ERROR in test %d(mul), rn = %d, n = %d, k = %d\n",
		  test, (int) rn, (int) n, (int) k);
	  if (pp[-1] != p_before)
	    {
	      printf ("before pp:"); mpn_dump (pp - 1, 1);
	      printf ("keep:   "); mpn_dump (&p_before, 1);
	    }
	  if (pp[rn + 1] != p_after)
	    {
	      printf ("after pp:"); mpn_dump (pp + rn + 1, 1);
	      printf ("keep:   "); mpn_dump (&p_after, 1);
	    }
	  if (scratch[-1] != s_before)
	    {
	      printf ("before scratch:"); mpn_dump (scratch - 1, 1);
	      printf ("keep:   "); mpn_dump (&s_before, 1);
	    }
	  if (scratch[itch] != s_after)
	    {
	      printf ("after scratch:"); mpn_dump (scratch + itch, 1);
	      printf ("keep:   "); mpn_dump (&s_after, 1);
	    }
	  mpn_dump (ap, rn + 1);
	  mpn_dump (pp, rn + 1);
	  mpn_dump (refp, rn + 1);

	  abort();
	}
    }
  TMP_FREE;
  tests_end ();
  return 0;
}
