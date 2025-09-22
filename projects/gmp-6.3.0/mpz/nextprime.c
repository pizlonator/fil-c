/* mpz_nextprime(p,t) - compute the next prime > t and store that in p.

Copyright 1999-2001, 2008, 2009, 2012, 2020-2022 Free Software
Foundation, Inc.

Contributed to the GNU project by Niels Möller and Torbjorn Granlund.
Improved by Seth Troisi.

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

#include <string.h>

#include "gmp-impl.h"
#include "longlong.h"

/*********************************************************/
/* Section sieve: sieving functions and tools for primes */
/*********************************************************/

static mp_limb_t
n_to_bit (mp_limb_t n) { return ((n-5)|1)/3U; }

static mp_size_t
primesieve_size (mp_limb_t n) { return n_to_bit(n) / GMP_LIMB_BITS + 1; }


static const unsigned char primegap_small[] =
{
  2,2,4,2,4,2,4,6,2,6,4,2,4,6,6,2,6,4,2,6,4,6,8,4,2,4,2,4,14,4,6,
  2,10,2,6,6,4,6,6,2,10,2,4,2,12,12,4,2,4,6,2,10,6,6,6,2,6,4,2,10,14,4,2,
  4,14,6,10,2,4,6,8,6,6,4,6,8,4,8,10,2,10,2,6,4,6,8,4,2,4,12,8,4,8,4,6,
  12,2,18,6,10
};

#define NUMBER_OF_PRIMES 100
#define LAST_PRIME 557
/* NP_SMALL_LIMIT = prevprime (LAST_PRIME ^ 2) */
#define NP_SMALL_LIMIT 310243

static unsigned long
calculate_sievelimit(mp_bitcnt_t nbits) {
  unsigned long sieve_limit;

  /* Estimate a good sieve bound. Based on derivative of
   *   Merten's 3rd theorem * avg gap * cost of mod
   *      vs
   *   Cost of PRP test O(N^2.55)
   */
  if (nbits < 12818)
    {
      mpz_t tmp;
      /* sieve_limit ~= nbits ^ (5/2) / 124 */
      mpz_init (tmp);
      mpz_ui_pow_ui (tmp, nbits, 5);
      mpz_tdiv_q_ui(tmp, tmp, 124*124);
      /* tmp < 12818^5/(124*124) < 2^55 < 2^64 */
      mpz_sqrt (tmp, tmp);

      sieve_limit = mpz_get_ui(tmp);
      mpz_clear (tmp);
    }
  else
    {
      /* Larger threshold is faster but takes (n/ln(n) + n/24) memory.
       * For 33,000 bits limitting to 150M is ~12% slower than using the
       * optimal 1.5G sieve_limit.
       */
      sieve_limit = 150000001;
    }

  ASSERT (1000 < sieve_limit && sieve_limit <= 150000001);
  return sieve_limit;
}

static unsigned
findnext_small (unsigned t, short diff)
{
  /* For diff= 2, expect t = 1 if operand was negative.
   * For diff=-2, expect t >= 3
   */
  ASSERT (t >= 3 || (diff > 0 && t >= 1));
  ASSERT (t < NP_SMALL_LIMIT);

  /* Start from next candidate (2 or odd) */
  t = diff > 0 ?
    (t + 1) | (t != 1) :
    ((t - 2) | 1) + (t == 3);

  for (; ; t += diff)
    {
      unsigned prime = 3;
      for (int i = 0; ; prime += primegap_small[i++])
	{
	  unsigned q, r;
	  q = t / prime;
	  r = t - q * prime; /* r = t % prime; */
	  if (q < prime)
	    return t;
	  if (r == 0)
	    break;
	  ASSERT (i < NUMBER_OF_PRIMES);
	}
    }
}

static int
findnext (mpz_ptr p,
          unsigned long(*negative_mod_ui)(const mpz_t, unsigned long),
          void(*increment_ui)(mpz_t, const mpz_t, unsigned long))
{
  char *composite;
  const unsigned char *primegap;
  unsigned long prime_limit;
  mp_size_t pn;
  mp_bitcnt_t nbits;
  int i, m;
  unsigned odds_in_composite_sieve;
  TMP_DECL;

  TMP_MARK;
  pn = SIZ(p);
  MPN_SIZEINBASE_2EXP(nbits, PTR(p), pn, 1);
  /* Smaller numbers handled earlier */
  ASSERT (nbits >= 3);
  /* Make p odd */
  PTR(p)[0] |= 1;

  if (nbits / 2 <= NUMBER_OF_PRIMES)
    {
      primegap = primegap_small;
      prime_limit = nbits / 2;
    }
  else
    {
      unsigned long sieve_limit;
      mp_limb_t *sieve;
      unsigned char *primegap_tmp;
      unsigned long last_prime;

      /* sieve numbers up to sieve_limit and save prime count */
      sieve_limit = calculate_sievelimit(nbits);
      sieve = TMP_ALLOC_LIMBS (primesieve_size (sieve_limit));
      prime_limit = gmp_primesieve(sieve, sieve_limit);

      /* TODO: Storing (prime - last_prime)/2 would allow this to go
	 up to the gap 304599508537+514=304599509051 .
	 With the current code our limit is 436273009+282=436273291 */
      ASSERT (sieve_limit < 436273291);
      /* THINK: Memory used by both sieve and primegap_tmp is kept
	 allocated, but they may overlap if primegap is filled from
	 larger down to smaller primes...
       */

      /* Needed to avoid assignment of read-only location */
      primegap_tmp = TMP_ALLOC_TYPE (prime_limit, unsigned char);
      primegap = primegap_tmp;

      i = 0;
      last_prime = 3;
      /* THINK: should we get rid of sieve_limit and use (i < prime_limit)? */
      for (mp_limb_t j = 4, *sp = sieve; j < sieve_limit; j += GMP_LIMB_BITS * 3)
	for (mp_limb_t b = j, x = ~ *(sp++); x != 0; b += 3, x >>= 1)
	  if (x & 1)
	    {
	      mp_limb_t prime = b | 1;
	      primegap_tmp[i++] = prime - last_prime;
	      last_prime = prime;
	    }

      /* Both primesieve and prime_limit ignore the first two primes. */
      ASSERT(i == prime_limit);
    }

  if (nbits <= 32)
    odds_in_composite_sieve = 336 / 2;
  else if (nbits <= 64)
    odds_in_composite_sieve = 1550 / 2;
  else
    /* Corresponds to a merit 14 prime_gap, which is rare. */
    odds_in_composite_sieve = 5 * nbits;

  /* composite[2*i] stores if p+2*i is a known composite */
  composite = TMP_ALLOC_TYPE (odds_in_composite_sieve, char);

  for (;;)
    {
      unsigned long difference;
      unsigned long incr, prime;
      int primetest;

      memset (composite, 0, odds_in_composite_sieve);
      prime = 3;
      for (i = 0; i < prime_limit; i++)
        {
          /* Distance to next multiple of prime */
          m = negative_mod_ui(p, prime);
          /* Only care about odd multiplies of prime. */
          if (m & 1)
            m += prime;
          m >>= 1;

          /* Mark off any composites in sieve */
          for (; m < odds_in_composite_sieve; m += prime)
            composite[m] = 1;
          prime += primegap[i];
        }

      difference = 0;
      for (incr = 0; incr < odds_in_composite_sieve; difference += 2, incr += 1)
        {
          if (composite[incr])
            continue;

          increment_ui(p, p, difference);
          difference = 0;

          /* Miller-Rabin test */
          primetest = mpz_millerrabin (p, 25);
          if (primetest)
	    {
	      TMP_FREE;
	      return primetest;
	    }
        }

      /* Sieve next segment, very rare */
      increment_ui(p, p, difference);
  }
}

void
mpz_nextprime (mpz_ptr p, mpz_srcptr n)
{
  /* Handle negative and small numbers */
  if (mpz_cmp_ui (n, NP_SMALL_LIMIT) < 0)
    {
      ASSERT (NP_SMALL_LIMIT < UINT_MAX);
      mpz_set_ui (p, findnext_small (SIZ (n) > 0 ? mpz_get_ui (n) : 1, +2));
      return;
    }

  /* First odd greater than n */
  mpz_add_ui (p, n, 1);

  findnext(p, mpz_cdiv_ui, mpz_add_ui);
}

int
mpz_prevprime (mpz_ptr p, mpz_srcptr n)
{
  /* Handle negative and small numbers */
  if (mpz_cmp_ui (n, 2) <= 0)
    return 0;

  if (mpz_cmp_ui (n, NP_SMALL_LIMIT) < 0)
    {
      ASSERT (NP_SMALL_LIMIT < UINT_MAX);
      mpz_set_ui (p, findnext_small (mpz_get_ui (n), -2));
      return 2;
    }

  /* First odd less than n */
  mpz_sub_ui (p, n, 2);

  return findnext(p, mpz_tdiv_ui, mpz_sub_ui);
}

