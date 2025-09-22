/* mpz_stronglucas(n, t1, t2) -- An implementation of the strong Lucas
   primality test on n, using parameters as suggested by the BPSW test.

   THE FUNCTIONS IN THIS FILE ARE FOR INTERNAL USE ONLY.  THEY'RE ALMOST
   CERTAIN TO BE SUBJECT TO INCOMPATIBLE CHANGES OR DISAPPEAR COMPLETELY IN
   FUTURE GNU MP RELEASES.

Copyright 2018, 2020 Free Software Foundation, Inc.

Contributed by Marco Bodrato.

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

/* Returns an approximation of the sqare root of x.
 * It gives:
 *   limb_apprsqrt (x) ^ 2 <= x < (limb_apprsqrt (x)+1) ^ 2
 * or
 *   x <= limb_apprsqrt (x) ^ 2 <= x * 9/8
 */
static mp_limb_t
limb_apprsqrt (mp_limb_t x)
{
  int s;

  ASSERT (x > 2);
  count_leading_zeros (s, x);
  s = (GMP_LIMB_BITS - s) >> 1;
  return ((CNST_LIMB(1) << (s - 1)) + (x >> 1 >> s));
}

static int
mpz_oddjacobi_ui (mpz_t b, mp_limb_t a)
{
  mp_limb_t  b_rem;
  int        result_bit1;

  ASSERT (a & 1);
  ASSERT (a > 1);
  ASSERT (SIZ (b) > 0);
  ASSERT ((*PTR (b) & 1) == 1);

  result_bit1 = 0;
  JACOBI_MOD_OR_MODEXACT_1_ODD (result_bit1, b_rem, PTR (b), SIZ (b), a);
  if (UNLIKELY (b_rem == 0))
    return 0;
  else
    return mpn_jacobi_base (b_rem, a, result_bit1);
}


/* Performs strong Lucas' test on x, with parameters suggested */
/* for the BPSW test. Qk and V are passed to recycle variables. */
/* Requires GCD (x,6) = 1.*/
int
mpz_stronglucas (mpz_srcptr x, mpz_ptr V, mpz_ptr Qk)
{
  mp_bitcnt_t b0;
  mpz_t n;
  mp_limb_t D; /* The absolute value is stored. */
  mp_limb_t g;
  long Q;
  mpz_t T1, T2;

  /* Test on the absolute value. */
  mpz_roinit_n (n, PTR (x), ABSIZ (x));

  ASSERT (mpz_odd_p (n));
  /* ASSERT (mpz_gcd_ui (NULL, n, 6) == 1);	*/
#if GMP_NUMB_BITS % 16 == 0
  /* (2^12 - 1) | (2^{GMP_NUMB_BITS*3/4} - 1)	*/
  g = mpn_mod_34lsub1 (PTR (n), SIZ (n));
  /* (2^12 - 1) = 3^2 * 5 * 7 * 13		*/
  ASSERT (g % 3 != 0 && g % 5 != 0 && g % 7 != 0);
  if ((g % 5 & 2) != 0)
    /* (5/n) = -1, iff n = 2 or 3 (mod 5)	*/
    /* D = 5; Q = -1 */
    return mpn_strongfibo (PTR (n), SIZ (n), PTR (V));
  else if (! POW2_P (g % 7))
    /* (-7/n) = -1, iff n = 3,5 or 6 (mod 7)	*/
    D = 7; /* Q = 2 */
    /* (9/n) = -1, never: 9 = 3^2	*/
  else if (mpz_oddjacobi_ui (n, 11) == -1)
    /* (-11/n) = (n/11)	*/
    D = 11; /* Q = 3 */
  else if ((((g % 13 - (g % 13 >> 3)) & 7) > 4) ||
	   (((g % 13 - (g % 13 >> 3)) & 7) == 2))
    /* (13/n) = -1, iff n = 2,5,6,7,8 or 11 (mod 13)	*/
    D = 13; /* Q = -3 */
  else if (g % 3 == 2)
    /* (-15/n) = (n/15) = (n/5)*(n/3)	*/
    /* Here, (n/5) = 1, and		*/
    /* (n/3) = -1, iff n = 2 (mod 3)	*/
    D = 15; /* Q = 4 */
#if GMP_NUMB_BITS % 32 == 0
  /* (2^24 - 1) | (2^{GMP_NUMB_BITS*3/4} - 1)	*/
  /* (2^24 - 1) = (2^12 - 1) * 17 * 241		*/
  else if (! POW2_P (g % 17) && ! POW2_P (17 - g % 17))
    /* (17/n) = -1, iff n != +-1,+-2,+-4,+-8 (mod 17)	*/
    D = 17; /* Q = -4 */
#endif
#else
  if (mpz_oddjacobi_ui (n, 5) == -1)
    return mpn_strongfibo (PTR (n), SIZ (n), PTR (V));
#endif
  else
  {
    mp_limb_t maxD;
    int jac;

    /* n is odd, to possibly be a square, n % 8 = 1 is needed. */
    if (((*PTR (n) & 6) == 0) && UNLIKELY (mpz_perfect_square_p (n)))
      return 0; /* A square is composite. */

    /* Check Ds up to square root (in case, n is prime)
       or avoid overflows */
    if (SIZ (n) == 1)
      maxD = limb_apprsqrt (* PTR (n));
    else if (BITS_PER_ULONG >= GMP_NUMB_BITS && SIZ (n) == 2)
      mpn_sqrtrem (&maxD, (mp_ptr) NULL, PTR (n), 2);
    else
      maxD = GMP_NUMB_MAX;
    maxD = MIN (maxD, ULONG_MAX);

    unsigned Ddiff = 2;
#if GMP_NUMB_BITS % 16 == 0
    const unsigned D2 = 6;
#if GMP_NUMB_BITS % 32 == 0
    D = 19;
    Ddiff = 4;
#else
    D = 17;
#endif
#else
    const unsigned D2 = 4;
    D = 7;
#endif

    /* Search a D such that (D/n) = -1 in the sequence 5,-7,9,-11,..	*/
    /* For those Ds we have (D/n) = (n/|D|)	*/
    /* FIXME: Should we loop only on prime Ds?	*/
    /* The only interesting composite D is 15, because 3 is not tested.	*/
    for (;;)
      {
	jac = mpz_oddjacobi_ui (n, D);
	if (jac != 1)
	  break;
	if (UNLIKELY (D >= maxD))
	  return 1;
	D += Ddiff;
	Ddiff = D2 - Ddiff;
      }

    if (UNLIKELY (jac == 0))
      return 0;
  }

  /* D= P^2 - 4Q; P = 1; Q = (1-D)/4 */
  Q = (D & 2) ? (D >> 2) + 1 : -(long) (D >> 2);
  /* ASSERT (mpz_si_kronecker ((D & 2) ? NEG_CAST (long, D) : D, n) == -1); */

  /* n-(D/n) = n+1 = d*2^{b0}, with d = (n>>b0) | 1 */
  b0 = mpz_scan0 (n, 0);

  mpz_init (T1);
  mpz_init (T2);

  /* If Ud != 0 && Vd != 0 */
  if (mpz_lucas_mod (V, Qk, Q, b0, n, T1, T2) == 0)
    if (LIKELY (--b0 != 0))
      for (;;)
	{
	  /* V_{2k} <- V_k ^ 2 - 2Q^k */
	  mpz_mul (T2, V, V);
	  mpz_submul_ui (T2, Qk, 2);
	  mpz_tdiv_r (V, T2, n);
	  if (SIZ (V) == 0 || UNLIKELY (--b0 == 0))
	    break;
	  /* Q^{2k} = (Q^k)^2 */
	  mpz_mul (T2, Qk, Qk);
	  mpz_tdiv_r (Qk, T2, n);
	}

  mpz_clear (T1);
  mpz_clear (T2);

  return (b0 != 0);
}
