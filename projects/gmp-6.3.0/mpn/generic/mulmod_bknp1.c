/* Mulptiplication mod B^n+1, for small operands.

   Contributed to the GNU project by Marco Bodrato.

   THE FUNCTIONS IN THIS FILE ARE INTERNAL WITH MUTABLE INTERFACES.  IT IS ONLY
   SAFE TO REACH THEM THROUGH DOCUMENTED INTERFACES.  IN FACT, IT IS ALMOST
   GUARANTEED THAT THEY WILL CHANGE OR DISAPPEAR IN A FUTURE GNU MP RELEASE.

Copyright 2020-2022 Free Software Foundation, Inc.

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

#ifndef MOD_BKNP1_USE11
#define MOD_BKNP1_USE11 ((GMP_NUMB_BITS % 8 != 0) && (GMP_NUMB_BITS % 2 == 0))
#endif
#ifndef MOD_BKNP1_ONLY3
#define MOD_BKNP1_ONLY3 0
#endif

/* {rp, (k - 1) * n} = {op, k * n + 1} % (B^{k*n}+1) / (B^n+1) */
static void
_mpn_modbknp1dbnp1_n (mp_ptr rp, mp_srcptr op, mp_size_t n, unsigned k)
{
  mp_limb_t hl;
  mp_srcptr hp;
  unsigned i;

#if MOD_BKNP1_ONLY3
  ASSERT (k == 3);
  k = 3;
#endif
  ASSERT (k > 2);
  ASSERT (k % 2 == 1);

  --k;

  rp += k * n;
  op += k * n;
  hp = op;
  hl = hp[n]; /* initial op[k*n]. */
  ASSERT (hl < GMP_NUMB_MAX - 1);

#if MOD_BKNP1_ONLY3 == 0
  /* The first MPN_INCR_U (rp + n, 1, cy); in the loop should be
     rp[n] = cy;						*/
  *rp = 0;
#endif

  i = k >> 1;
  do
   {
     mp_limb_t cy, bw;
     rp -= n;
     op -= n;
     cy = hl + mpn_add_n (rp, op, hp, n);
#if MOD_BKNP1_ONLY3
     rp[n] = cy;
#else
     MPN_INCR_U (rp + n, (k - i * 2) * n + 1, cy);
#endif
     rp -= n;
     op -= n;
     bw = hl + mpn_sub_n (rp, op, hp, n);
     MPN_DECR_U (rp + n, (k - i * 2 + 1) * n + 1, bw);
   }
  while (--i != 0);

  for (; (hl = *(rp += k * n)) != 0; ) /* Should run only once... */
    {
      *rp = 0;
      i = k >> 1;
      do
	{
	  rp -= n;
	  MPN_INCR_U (rp, (k - i * 2 + 1) * n + 1, hl);
	  rp -= n;
	  MPN_DECR_U (rp, (k - i * 2 + 2) * n + 1, hl);
	}
      while (--i != 0);
    }
}

static void
_mpn_modbnp1_pn_ip (mp_ptr r, mp_size_t n, mp_limb_t h)
{
  ASSERT (r[n] == h);

  /* Fully normalise */
  MPN_DECR_U (r, n + 1, h);
  h -= r[n];
  r[n] = 0;
  MPN_INCR_U (r, n + 1, h);
}

static void
_mpn_modbnp1_neg_ip (mp_ptr r, mp_size_t n, mp_limb_t h)
{
  r[n] = 0;
  MPN_INCR_U (r, n + 1, -h);
  if (UNLIKELY (r[n] != 0))
    _mpn_modbnp1_pn_ip (r, n, 1);
}

static void
_mpn_modbnp1_nc_ip (mp_ptr r, mp_size_t n, mp_limb_t h)
{
  if (h & GMP_NUMB_HIGHBIT) /* This means h < 0 */
    {
      _mpn_modbnp1_neg_ip (r, n, h);
    }
  else
    {
      r[n] = h;
      if (h)
	_mpn_modbnp1_pn_ip(r, n, h);
    }
}

/* {rp, rn + 1} = {op, on} mod (B^{rn}+1) */
/* Used when rn < on < 2*rn. */
static void
_mpn_modbnp1 (mp_ptr rp, mp_size_t rn, mp_srcptr op, mp_size_t on)
{
  mp_limb_t bw;

#if 0
  if (UNLIKELY (on <= rn))
    {
      MPN_COPY (rp, op, on);
      MPN_ZERO (rp + on, rn - on);
      return;
    }
#endif

  ASSERT (on > rn);
  ASSERT (on <= 2 * rn);

  bw = mpn_sub (rp, op, rn, op + rn, on - rn);
  rp[rn] = 0;
  MPN_INCR_U (rp, rn + 1, bw);
}

/* {rp, rn + 1} = {op, k * rn + 1} % (B^{rn}+1) */
/* With odd k >= 3. */
static void
_mpn_modbnp1_kn (mp_ptr rp, mp_srcptr op, mp_size_t rn, unsigned k)
{
  mp_limb_t cy;

#if MOD_BKNP1_ONLY3
  ASSERT (k == 3);
  k = 3;
#endif
  ASSERT (k & 1);
  k >>= 1;
  ASSERT (0 < k && k < GMP_NUMB_HIGHBIT - 3);
  ASSERT (op[(1 + 2 * k) * rn] < GMP_NUMB_HIGHBIT - 2 - k);

  cy = - mpn_sub_n (rp, op, op + rn, rn);
  for (;;) {
    op += 2 * rn;
    cy += mpn_add_n (rp, rp, op, rn);
    if (--k == 0)
      break;
    cy -= mpn_sub_n (rp, rp, op + rn, rn);
  };

  cy += op[rn];
  _mpn_modbnp1_nc_ip (rp, rn, cy);
}

/* For the various mpn_divexact_byN here, fall back to using either
   mpn_pi1_bdiv_q_1 or mpn_divexact_1.  The former has less overhead and is
   faster if it is native.  For now, since mpn_divexact_1 is native on
   platforms where mpn_pi1_bdiv_q_1 does not yet exist, do not use
   mpn_pi1_bdiv_q_1 unconditionally.  FIXME.  */

#ifndef mpn_divexact_by5
#if HAVE_NATIVE_mpn_pi1_bdiv_q_1
#define BINVERT_5 \
  ((((GMP_NUMB_MAX >> (GMP_NUMB_BITS % 4)) / 5 * 3 << 3) + 5) & GMP_NUMB_MAX)
#define mpn_divexact_by5(dst,src,size) mpn_pi1_bdiv_q_1(dst,src,size,5,BINVERT_5,0)
#else
#define mpn_divexact_by5(dst,src,size) mpn_divexact_1(dst,src,size,5)
#endif
#endif

#ifndef mpn_divexact_by7
#if HAVE_NATIVE_mpn_pi1_bdiv_q_1
#define BINVERT_7 \
  ((((GMP_NUMB_MAX >> (GMP_NUMB_BITS % 3)) / 7 * 3 << 4) + 7) & GMP_NUMB_MAX)
#define mpn_divexact_by7(dst,src,size) mpn_pi1_bdiv_q_1(dst,src,size,7,BINVERT_7,0)
#else
#define mpn_divexact_by7(dst,src,size) mpn_divexact_1(dst,src,size,7)
#endif
#endif

#ifndef mpn_divexact_by11
#if HAVE_NATIVE_mpn_pi1_bdiv_q_1
#define BINVERT_11 \
  ((((GMP_NUMB_MAX >> (GMP_NUMB_BITS % 10)) / 11 << 5) + 3) & GMP_NUMB_MAX)
#define mpn_divexact_by11(dst,src,size) mpn_pi1_bdiv_q_1(dst,src,size,11,BINVERT_11,0)
#else
#define mpn_divexact_by11(dst,src,size) mpn_divexact_1(dst,src,size,11)
#endif
#endif

#ifndef mpn_divexact_by13
#if HAVE_NATIVE_mpn_pi1_bdiv_q_1
#define BINVERT_13 \
  ((((GMP_NUMB_MAX >> (GMP_NUMB_BITS % 12)) / 13 * 3 << 14) + 3781) & GMP_NUMB_MAX)
#define mpn_divexact_by13(dst,src,size) mpn_pi1_bdiv_q_1(dst,src,size,13,BINVERT_13,0)
#else
#define mpn_divexact_by13(dst,src,size) mpn_divexact_1(dst,src,size,13)
#endif
#endif

#ifndef mpn_divexact_by17
#if HAVE_NATIVE_mpn_pi1_bdiv_q_1
#define BINVERT_17 \
  ((((GMP_NUMB_MAX >> (GMP_NUMB_BITS % 8)) / 17 * 15 << 7) + 113) & GMP_NUMB_MAX)
#define mpn_divexact_by17(dst,src,size) mpn_pi1_bdiv_q_1(dst,src,size,17,BINVERT_17,0)
#else
#define mpn_divexact_by17(dst,src,size) mpn_divexact_1(dst,src,size,17)
#endif
#endif

/* Thanks to Chinese remainder theorem, store
   in {rp, k*n+1} the value mod (B^(k*n)+1), given
   {ap, k*n+1} mod ((B^(k*n)+1)/(B^n+1)) and
   {bp, n+1} mod (B^n+1) .
   {tp, n+1} is a scratch area.
   tp == rp or rp == ap are possible.
*/
static void
_mpn_crt (mp_ptr rp, mp_srcptr ap, mp_srcptr bp,
	  mp_size_t n, unsigned k, mp_ptr tp)
{
  mp_limb_t mod;
  unsigned i;

#if MOD_BKNP1_ONLY3
  ASSERT (k == 3);
  k = 3;
#endif
  _mpn_modbnp1_kn (tp, ap, n, k);
  if (mpn_sub_n (tp, bp, tp, n + 1))
    _mpn_modbnp1_neg_ip (tp, n, tp[n]);

#if MOD_BKNP1_USE11
  if (UNLIKELY (k == 11))
    {
      ASSERT (GMP_NUMB_BITS % 2 == 0);
      /* mod <- -Mod(B^n+1,11)^-1 */
      mod = n * (GMP_NUMB_BITS % 5) % 5;
      if ((mod > 2) || UNLIKELY (mod == 0))
	mod += 5;

      mod *= mpn_mod_1 (tp, n + 1, 11);
    }
  else
#endif
    {
#if GMP_NUMB_BITS % 8 == 0
  /* (2^6 - 1) | (2^{GMP_NUMB_BITS*3/4} - 1)	*/
  /* (2^6 - 1) = 3^2 * 7			*/
  mod = mpn_mod_34lsub1 (tp, n + 1);
  ASSERT ((GMP_NUMB_MAX >> (GMP_NUMB_BITS >> 2)) % k == 0);
  /* (2^12 - 1) = 3^2 * 5 * 7 * 13		*/
  /* (2^24 - 1) = 3^2 * 5 * 7 * 13 * 17 * 241	*/
  ASSERT (k == 3 || k == 5 || k == 7 || k == 13 || k == 17);

#if GMP_NUMB_BITS % 3 != 0
  if (UNLIKELY (k != 3))
    {
      ASSERT ((GMP_NUMB_MAX % k == 0) || (n % 3 != 0));
      if ((GMP_NUMB_BITS % 16 == 0) && LIKELY (k == 5))
	mod <<= 1; /* k >> 1 = 1 << 1 */
      else if ((GMP_NUMB_BITS % 16 != 0) || LIKELY (k == 7))
	mod <<= (n << (GMP_NUMB_BITS % 3 >> 1)) % 3;
      else if ((GMP_NUMB_BITS % 32 != 0) || LIKELY (k == 13))
	mod *= ((n << (GMP_NUMB_BITS % 3 >> 1)) % 3 == 1) ? 3 : 9;
      else /* k == 17 */
	mod <<= 3; /* k >> 1 = 1 << 3 */
#if 0
      if ((GMP_NUMB_BITS == 8) /* && (k == 7) */ ||
	  (GMP_NUMB_BITS == 16) && (k == 13))
	mod = ((mod & (GMP_NUMB_MAX >> (GMP_NUMB_BITS >> 2))) +
	       (mod >> (3 * GMP_NUMB_BITS >> 2)));
#endif
    }
#else
  ASSERT (GMP_NUMB_MAX % k == 0);
  /* 2^{GMP_NUMB_BITS} - 1	= 0 (mod k) */
  /* 2^{GMP_NUMB_BITS}		= 1 (mod k) */
  /* 2^{n*GMP_NUMB_BITS} + 1	= 2 (mod k) */
  /* -2^{-1}	= k >> 1 (mod k) */
  mod *= k >> 1;
#endif
#else
  ASSERT_ALWAYS (k == 0); /* Not implemented, should not be used. */
#endif
    }

  MPN_INCR_U (tp, n + 1, mod);
  tp[n] += mod;

  if (LIKELY (k == 3))
    ASSERT_NOCARRY (mpn_divexact_by3 (tp, tp, n + 1));
  else if ((GMP_NUMB_BITS % 16 == 0) && LIKELY (k == 5))
    mpn_divexact_by5 (tp, tp, n + 1);
  else if (((! MOD_BKNP1_USE11) && (GMP_NUMB_BITS % 16 != 0))
	   || LIKELY (k == 7))
    mpn_divexact_by7 (tp, tp, n + 1);
#if MOD_BKNP1_USE11
  else if (k == 11)
    mpn_divexact_by11 (tp, tp, n + 1);
#endif
  else if ((GMP_NUMB_BITS % 32 != 0) || LIKELY (k == 13))
    mpn_divexact_by13 (tp, tp, n + 1);
  else /* (k == 17) */
    mpn_divexact_by17 (tp, tp, n + 1);

  rp += k * n;
  ap += k * n; /* tp - 1 */

  rp -= n;
  ap -= n;
  ASSERT_NOCARRY (mpn_add_n (rp, ap, tp, n + 1));

  i = k >> 1;
  do
   {
      mp_limb_t cy, bw;
      rp -= n;
      ap -= n;
      bw = mpn_sub_n (rp, ap, tp, n) + tp[n];
      MPN_DECR_U (rp + n, (k - i * 2) * n + 1, bw);
      rp -= n;
      ap -= n;
      cy = mpn_add_n (rp, ap, tp, n) + tp[n];
      MPN_INCR_U (rp + n, (k - i * 2 + 1) * n + 1, cy);
    }
  while (--i != 0);

  /* if (LIKELY (rp[k * n])) */
    _mpn_modbnp1_pn_ip (rp, k * n, rp[k * n]);
}


static void
_mpn_mulmod_bnp1_tp (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t rn,
		    mp_ptr tp)
{
  mp_limb_t cy;
  unsigned k;

  ASSERT (0 < rn);
  ASSERT ((ap[rn] | bp[rn]) <= 1);

  if (UNLIKELY (ap[rn] | bp[rn]))
    {
      if (ap[rn])
	cy = bp[rn] + mpn_neg (rp, bp, rn);
      else /* ap[rn] == 0 */
	cy = mpn_neg (rp, ap, rn);
    }
  else if (MPN_MULMOD_BKNP1_USABLE(rn, k, MUL_FFT_MODF_THRESHOLD / 3))
    {
      rn /= k;
      mpn_mulmod_bknp1 (rp, ap, bp, rn, k, tp);
      return;
    }
  else
    {
      mpn_mul_n (tp, ap, bp, rn);
      cy = mpn_sub_n (rp, tp, tp + rn, rn);
    }
  rp[rn] = 0;
  MPN_INCR_U (rp, rn + 1, cy);
}

/* {rp, kn + 1} = {ap, kn + 1} * {bp, kn + 1} % (B^kn + 1) */
/* tp must point to at least 4*(k-1)*n+1 limbs*/
void
mpn_mulmod_bknp1 (mp_ptr rp, mp_srcptr ap, mp_srcptr bp,
		  mp_size_t n, unsigned k, mp_ptr tp)
{
  mp_ptr hp;

#if MOD_BKNP1_ONLY3
  ASSERT (k == 3);
  k = 3;
#endif
  ASSERT (k > 2);
  ASSERT (k % 2 == 1);

  /* a % (B^{nn}+1)/(B^{nn/k}+1) */
  _mpn_modbknp1dbnp1_n (tp + (k - 1) * n * 2, ap, n, k);
  /* b % (B^{nn}+1)/(B^{nn/k}+1) */
  _mpn_modbknp1dbnp1_n (tp + (k - 1) * n * 3, bp, n, k);
  mpn_mul_n (tp, tp + (k - 1) * n * 2, tp + (k - 1) * n * 3, (k - 1) * n);
  _mpn_modbnp1 (tp, k * n, tp, (k - 1) * n * 2);

  hp = tp + k * n + 1;
  /* a % (B^{nn/k}+1) */
  ASSERT (ap[k * n] <= 1);
  _mpn_modbnp1_kn (hp, ap, n, k);
  /* b % (B^{nn/k}+1) */
  ASSERT (bp[k * n] <= 1);
  _mpn_modbnp1_kn (hp + n + 1, bp, n, k);
  _mpn_mulmod_bnp1_tp (hp + (n + 1) * 2, hp, hp + n + 1, n, hp + (n + 1) * 2);

  _mpn_crt (rp, tp, hp + (n + 1) * 2, n, k, hp);
}


static void
_mpn_sqrmod_bnp1_tp (mp_ptr rp, mp_srcptr ap, mp_size_t rn,
		    mp_ptr tp)
{
  mp_limb_t cy;
  unsigned k;

  ASSERT (0 < rn);

  if (UNLIKELY (ap[rn]))
    {
      ASSERT (ap[rn] == 1);
      *rp = 1;
      MPN_FILL (rp + 1, rn, 0);
      return;
    }
  else if (MPN_SQRMOD_BKNP1_USABLE(rn, k, MUL_FFT_MODF_THRESHOLD / 3))
    {
      rn /= k;
      mpn_sqrmod_bknp1 (rp, ap, rn, k, tp);
      return;
    }
  else
    {
      mpn_sqr (tp, ap, rn);
      cy = mpn_sub_n (rp, tp, tp + rn, rn);
    }
  rp[rn] = 0;
  MPN_INCR_U (rp, rn + 1, cy);
}

/* {rp, kn + 1} = {ap, kn + 1}^2 % (B^kn + 1) */
/* tp must point to at least 3*(k-1)*n+1 limbs*/
void
mpn_sqrmod_bknp1 (mp_ptr rp, mp_srcptr ap,
		  mp_size_t n, unsigned k, mp_ptr tp)
{
  mp_ptr hp;

#if MOD_BKNP1_ONLY3
  ASSERT (k == 3);
  k = 3;
#endif
  ASSERT (k > 2);
  ASSERT (k % 2 == 1);

  /* a % (B^{nn}+1)/(B^{nn/k}+1) */
  _mpn_modbknp1dbnp1_n (tp + (k - 1) * n * 2, ap, n, k);
  mpn_sqr (tp, tp + (k - 1) * n * 2, (k - 1) * n);
  _mpn_modbnp1 (tp, k * n, tp, (k - 1) * n * 2);

  hp = tp + k * n + 1;
  /* a % (B^{nn/k}+1) */
  ASSERT (ap[k * n] <= 1);
  _mpn_modbnp1_kn (hp, ap, n, k);
  _mpn_sqrmod_bnp1_tp (hp + (n + 1), hp, n, hp + (n + 1));

  _mpn_crt (rp, tp, hp + (n + 1), n, k, hp);
}
