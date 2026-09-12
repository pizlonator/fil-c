/* Correctly-rounded inverse hyperbolic sine function.  Binary64 version.

Copyright (c) 2023-2026 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary64/asinh/asinh.c, revision cd653cf7).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <array_length.h>
#include <stdint.h>
#include <math.h>
#include <libm-alias-double.h>
#include "math_config.h"
#include "s_asincosh_data.h"
#include "ddcoremath.h"

static double __attribute__ ((noinline)) as_asinh_refine (double, double,
							  double, double);
static double __attribute__ ((noinline))
as_asinh_zero (double x, double x2h, double x2l)
{
  static const double ch[][2]
      = { { -0x1.5555555555555p-3, -0x1.5555555555555p-57 },
	  { 0x1.3333333333333p-4, 0x1.99999999949dfp-59 },
	  { -0x1.6db6db6db6db7p-5, 0x1.2492496091b0cp-60 },
	  { 0x1.f1c71c71c71c7p-6, 0x1.c71a35cfa0671p-62 },
	  { -0x1.6e8ba2e8ba2e9p-6, 0x1.17f937248cf81p-60 },
	  { 0x1.1c4ec4ec4ec4fp-6, -0x1.74e3c1dfd4c3dp-60 },
	  { -0x1.c999999999977p-7, -0x1.38e7a467ecc55p-61 },
	  { 0x1.7a87878786c7ep-7, 0x1.a83c7bace55ebp-61 },
	  { -0x1.3fde50d764083p-7, -0x1.d024df7fa0542p-61 },
	  { 0x1.12ef3ceae4d12p-7, -0x1.ba9c13deb261fp-61 },
	  { -0x1.df3bd104aa267p-8, -0x1.546da9bc5b32ap-62 },
	  { 0x1.a685fc5de7a04p-8, 0x1.40d284a1d67f9p-62 } };
  static const double cl[]
      = { -0x1.7828d553ec8p-8, 0x1.51712f7bee368p-8, -0x1.2e6d98527bcc6p-8,
	  0x1.0095da47b392cp-8, -0x1.3b92d6368192cp-9 };
  double y2
      = x2h
	* (cl[0]
	   + x2h * (cl[1] + x2h * (cl[2] + x2h * (cl[3] + x2h * (cl[4])))));
  double y1 = polydd (x2h, x2l, 12, ch, &y2);

  y1 = muldd_acc (y1, y2, x2h, x2l, &y2);
  y1 = mulddd (y1, y2, x, &y2);
  double y0 = fasttwosum (x, y1, &y1);
  y1 = fasttwosum (y1, y2, &y2);

  uint64_t t = asuint64 (y1);
  if (__glibc_unlikely (!(t & (~UINT64_C (0) >> 12))))
    {
      uint64_t w = asuint64 (y2);
      if ((w ^ t) >> 63)
	t--;
      else
	t++;
      y1 = asdouble (t);
    }
  return y0 + y1;
}

double
__asinh (double x)
{
  double ax = fabs (x);
  uint64_t u = asuint64 (ax);;
  if (__glibc_unlikely (u < UINT64_C (0x3fbb000000000000)))
    { // |x| < 0x1.bp-4
      // for |x| < 0x1.7137449123ef7p-26, asinh(x) rounds to x to nearest
      // for |x| < 0x1p-1022 we have underflow but not for 0x1p-1022 (to
      // nearest)
      if (__glibc_unlikely (u < UINT64_C (0x3e57137449123ef7)))
	{ // |x| < 0x1.7137449123ef7p-26
	  if (__glibc_unlikely (!u))
	    return x;
	  double res = fma (-0x1p-60, x, x);
	  return __math_check_uflow_lt (res, 0x1p-1022);
	}
      double x2h = x * x, x2l = fma (x, x, -x2h);
      double x3h = x2h * x, sl;
      if (__glibc_unlikely (u < UINT64_C (0x3f93000000000000)))
	{ // |x| < 0x1.3p-6
	  if (__glibc_unlikely (u < UINT64_C (0x3f30000000000000)))
	    { // |x| < 0x1p-12
	      if (__glibc_unlikely (u < UINT64_C (0x3e5a000000000000)))
		{ // |x| < 0x1.ap-26
		  static const double cl[] = { -0x1.5555555555555p-3 };
		  sl = x3h * cl[0];
		}
	      else
		{
		  static const double cl[]
		      = { -0x1.5555555555555p-3, 0x1.3333327c57c6p-4 };
		  sl = x3h * (cl[0] + x2h * cl[1]);
		}
	    }
	  else
	    {
	      static const double cl[]
		  = { -0x1.5555555555555p-3, 0x1.333333332f2ffp-4,
		      -0x1.6db6d9a665159p-5, 0x1.f186866d775fp-6 };
	      sl = x3h * (cl[0] + x2h * (cl[1] + x2h * (cl[2] + x2h * cl[3])));
	    }
	}
      else // 0x1.3p-6 <= |x| < 0x1.bp-4
	{
	  /* p = x + cl[0]*x^3 + ... + cl[6]*x^15 is a minimax polynomial
	     with relative error < 2^-63.091 on [0x1.3p-6, 0x1.bp-4].
	     This branch (0x1.3p-6 <= x < 0x1.bp-4) was tested exhaustively
	     by Vincenzo Innocente (both with/without FMA).
	     All found failures were added to asinh.wc.  */
	  static const double cl[]
	      = { -0x1.5555555555555p-3, 0x1.333333333331p-4,
		  -0x1.6db6db6da466cp-5, 0x1.f1c71c2ea7be4p-6,
		  -0x1.6e8b651b09d72p-6, 0x1.1c309fc0e69c2p-6,
		  -0x1.bab7833c1ep-7 };
	  double c1 = cl[1] + x2h * cl[2];
	  double c3 = cl[3] + x2h * cl[4];
	  double c5 = cl[5] + x2h * cl[6];
	  double x4 = x2h * x2h;
	  sl = x3h * (cl[0] + x2h * (c1 + x4 * (c3 + x4 * c5)));
	}
      double eps = 0x1.79p-53 * x3h;
      double lb = x + (sl - eps), ub = x + (sl + eps);
      if (lb == ub)
	return lb;
      return as_asinh_zero (x, x2h, x2l);
    }
  // |x| >= 0x1.bp-4
  double x2h = 0, x2l = 0;
  double ah, al;
  int off = 0x3ff;
  if (__glibc_likely (u < UINT64_C (0x4190000000000000)))
    { // |x| < 0x1p+26
      double th, tl;
      x2h = x * x;
      x2l = fma (x, x, -x2h);
      if (__glibc_unlikely (u < UINT64_C (0x3ff0000000000000)))
	th = fasttwosum (1, x2h, &tl);
      else
	 th = fasttwosum (x2h, 1, &tl);
      tl += x2l;
      ah = sqrt (th);
      double rs = 0.5 / th;
      al = (tl - fma (ah, ah, -th)) * (rs * ah);
      ah = fasttwosum (ah, ax, &tl);
      al += tl;
    }
  else if (u < UINT64_C (0x4330000000000000))
    { // |x| < 0x1p+52
      ah = 2 * ax;
      al = 0.5 / ax;
    }
  else
    { // |x| >= 0x1p+52
      if (__glibc_unlikely (u >= UINT64_C (0x7ff0000000000000)))
	return x + x; // +-inf or nan
      off = 0x3fe;
      ah = ax;
      al = 0;
    }

  uint64_t t = asuint64 (ah);
  int ex = t >> 52, e = ex - off;
  t &= ~UINT64_C (0) >> 12;
  double ed = e;
  uint64_t i = t >> (52 - 5);
  int64_t d = t & (~UINT64_C (0) >> 17);
  uint64_t j
      = (t + ((uint64_t) B[i].c0 << 33) + ((int64_t) B[i].c1 * (d >> 16)))
	>> (52 - 10);
  t |= UINT64_C (0x3ff) << 52;
  int i1 = j >> 5, i2 = j & 0x1f;
  double r = r1[i1] * r2[i2], dx = fma (r, asdouble (t), -1), dx2 = dx * dx;
  double f
      = dx2 * ((c[0] + dx * c[1]) + dx2 * ((c[2] + dx * c[3]) + dx2 * c[4]));
  const double l2h = 0x1.62e42fefa38p-1, l2l = 0x1.ef35793c7673p-45;
  double lh = l2h * ed + (l1[i1][1] + l2[i2][1]);
  double ll = l2l * ed + l1[i1][0] + l2[i2][0] + al / ah + f;
  ll += dx;
  lh *= copysign (1, x);
  ll *= copysign (1, x);
  double eps = 1.63e-19;
  double lb = lh + (ll - eps), ub = lh + (ll + eps);
  if (lb == ub)
    return lb;
  if (ax < 0x1p-2)
    return as_asinh_zero (x, x2h, x2l);
  return as_asinh_refine (x, ah, al,
			  0x1.71547652b82fep+0 * fabs (lb));
}
libm_alias_double (__asinh, asinh)

static __attribute__ ((noinline)) double
as_asinh_database (double x, double f)
{
  static const double db[][3] = {
    { 0x1.00f9476450863p-2, 0x1.fcb35067f343cp-3, 0x1p-57 },
    { 0x1.1f0a79315b287p-2, 0x1.1b68aae88febap-2, 0x1p-56 },
    { 0x1.2b9618ff7acb7p-2, 0x1.27781d9aa4e25p-2, -0x1p-56 },
    { 0x1.389ef683f3aa7p-2, 0x1.33f52db6df1afp-2, 0x1p-56 },
    { 0x1.3b07e0c779ddap-2, 0x1.364303e1ad8f6p-2, 0x1p-56 },
    { 0x1.48441df33b6d3p-2, 0x1.42e385800f0a4p-2, 0x1p-56 },
    { 0x1.687bd068c1c1ep-2, 0x1.616cc75d49226p-2, -0x1p-56 },
    { 0x1.8740c4453a056p-2, 0x1.7e4f2ad132a1dp-2, 0x1p-56 },
    { 0x1.891acda11167ep-2, 0x1.8009d924a3ffdp-2, 0x1p-56 },
    { 0x1.bafc3479fc9ccp-2, 0x1.ae3773250e7d2p-2, 0x1p-56 },
    { 0x1.c59869f17b483p-2, 0x1.b7efa91915c95p-2, 0x1p-56 },
    { 0x1.c8be879787986p-2, 0x1.bad0485e0fe0ap-2, -0x1p-56 },
    { 0x1.e73b46abb01e1p-2, 0x1.d68039861ab53p-2, 0x1p-56 },
    { 0x1.ed6236da268bp-2, 0x1.dc0cb8f638126p-2, 0x1p-56 },
    { 0x1.f399ebafc1951p-2, 0x1.e1a4f519fab77p-2, -0x1p-56 },
    { 0x1.f70975ab0d471p-2, 0x1.e4bae8bcd6ea6p-2, 0x1p-56 },
    { 0x1.fbdd4a37760b7p-2, 0x1.e90f16eb88c09p-2, 0x1p-56 },
    { 0x1.fee72efb4bfddp-2, 0x1.ebc791a88bed8p-2, 0x1p-56 },
    { 0x1.02339d6bdb741p-1, 0x1.f0b2264e34555p-2, 0x1p-56 },
    { 0x1.09e7c831b1a23p-1, 0x1.fe694c3c89138p-2, 0x1p-56 },
    { 0x1.16d32c862fc3bp-1, 0x1.0a9c9334066dbp-1, -0x1p-55 },
    { 0x1.857954132083dp-1, 0x1.67425fe575c88p-1, -0x1p-55 },
    { 0x1.8a5c3b60f7e11p-1, 0x1.6b23ad4415a17p-1, -0x1p-55 },
    { 0x1.9740eb419dd04p-1, 0x1.754ab7535d47dp-1, 0x1p-55 },
    { 0x1.a16d9cc06011ap-1, 0x1.7d3755d851062p-1, -0x1p-55 },
    { 0x1.bb635be2213d1p-1, 0x1.91167cae3cfa9p-1, 0x1p-55 },
    { 0x1.d4b21ebf542fp-1, 0x1.a3fc7e4dd47d1p-1, -0x1p-55 },
    { 0x1.7b8516ffd2406p+0, 0x1.2f5d3b178914ap+0, 0x1p-54 },
    { 0x1.9295b9116e2e2p+0, 0x1.3bffa8863976p+0, 0x1p-54 },
    { 0x1.fedc65e32714p+0, 0x1.710f91e844f9bp+0, 0x1p-54 },
    { 0x1.57e377b3f0b4bp+1, 0x1.b6e2c73f41415p+0, 0x1p-54 },
    { 0x1.6056b06a21918p+3, 0x1.8c0a26d055288p+1, 0x1p-53 },
    { 0x1.843e1b5e5979cp+4, 0x1.f0f978201eb84p+1, 0x1p-53 },
    { 0x1.fee8f69c4cd25p+10, 0x1.0a19aebb51e9p+3, -0x1p-51 },
    { 0x1.0fbc6c02b1c9p+24, 0x1.16369cd53bb69p+4, 0x1p-50 },
  };
  double ax = fabs (x);
  int a = 0, b = array_length (db) - 1, m = (a + b) / 2;
  while (a <= b)
    { // binary search
      if (db[m][0] < ax)
	a = m + 1;
      else if (db[m][0] == ax)
	{
	  double sgn = copysign (1, x);
	  f = sgn * db[m][1] + sgn * db[m][2];
	  break;
	}
      else
	b = m - 1;
      m = (a + b) / 2;
    }
  return f;
}

static double
as_asinh_refine (double x, double zh, double zl, double a)
{
  static const double t1[]
      = { 0x1p+0,	  0x1.ea4afap-1, 0x1.d5818ep-1,	 0x1.c199bep-1,
	  0x1.ae89f98p-1, 0x1.9c4918p-1, 0x1.8ace54p-1,	 0x1.7a1147p-1,
	  0x1.6a09e68p-1, 0x1.5ab07ep-1, 0x1.4bfdad8p-1, 0x1.3dea65p-1,
	  0x1.306fe08p-1, 0x1.2387a7p-1, 0x1.172b84p-1,	 0x1.0b5587p-1,
	  0x1p-1 };
  static const double t2[]
      = { 0x1p+0,	  0x1.fe9d968p-1, 0x1.fd3c228p-1, 0x1.fbdba38p-1,
	  0x1.fa7c18p-1,  0x1.f91d8p-1,	  0x1.f7bfdbp-1,  0x1.f663278p-1,
	  0x1.f507658p-1, 0x1.f3ac948p-1, 0x1.f252b38p-1, 0x1.f0f9c2p-1,
	  0x1.efa1bfp-1,  0x1.ee4aaap-1,  0x1.ecf483p-1,  0x1.eb9f488p-1 };
  static const double t3[]
      = { 0x1p+0,	  0x1.ffe9d2p-1,  0x1.ffd3a58p-1, 0x1.ffbd798p-1,
	  0x1.ffa74e8p-1, 0x1.ff91248p-1, 0x1.ff7afb8p-1, 0x1.ff64d38p-1,
	  0x1.ff4eac8p-1, 0x1.ff38868p-1, 0x1.ff22618p-1, 0x1.ff0c3dp-1,
	  0x1.fef61ap-1,  0x1.fedff78p-1, 0x1.fec9d68p-1, 0x1.feb3b6p-1 };
  static const double t4[]
      = { 0x1p+0,	  0x1.fffe9dp-1,  0x1.fffd3ap-1,  0x1.fffbd78p-1,
	  0x1.fffa748p-1, 0x1.fff9118p-1, 0x1.fff7ae8p-1, 0x1.fff64cp-1,
	  0x1.fff4e9p-1,  0x1.fff386p-1,  0x1.fff2238p-1, 0x1.fff0c08p-1,
	  0x1.ffef5d8p-1, 0x1.ffedfa8p-1, 0x1.ffec98p-1,  0x1.ffeb35p-1 };
  static const double LL[4][17][3] = {
    {
	{ 0x0p+0, 0x0p+0, 0x0p+0 },
	{ 0x1.62e432b24p-6, -0x1.745af34bb54b8p-42, -0x1.17e3ec05cde7p-97 },
	{ 0x1.62e42e4a8p-5, 0x1.111a4eadf312p-44, 0x1.cff3027abb119p-93 },
	{ 0x1.0a2b233f1p-4, -0x1.88ac4ec78af8p-42, 0x1.4fa087ca75dfdp-93 },
	{ 0x1.62e43056cp-4, 0x1.6bd65e8b0b7p-46, -0x1.b18e160362c24p-95 },
	{ 0x1.bb9d3cbd6p-4, 0x1.de14aa55ec2bp-42, -0x1.c6ac3f1862a6bp-94 },
	{ 0x1.0a2b244dap-3, 0x1.94def487fea7p-42, -0x1.dead1a4581acfp-94 },
	{ 0x1.3687aa9b78p-3, 0x1.9cec9a50db22p-43, 0x1.34a70684f8e0ep-93 },
	{ 0x1.62e42fabap-3, -0x1.d69047a3aebp-44, -0x1.4e061f79144e2p-95 },
	{ 0x1.8f40b56d28p-3, 0x1.de7d755fd2e2p-42, 0x1.bdc7ecf001489p-94 },
	{ 0x1.bb9d3b61fp-3, 0x1.c14f1445b12p-46, 0x1.a1d78cbdc5b58p-93 },
	{ 0x1.e7f9c11f08p-3, -0x1.6e3e0000dae7p-43, 0x1.6a4559fadde98p-94 },
	{ 0x1.0a2b242ec4p-2, 0x1.bb7cf852a5fe8p-42, 0x1.a6aef11ee43bdp-93 },
	{ 0x1.205966c764p-2, 0x1.ad3a5f214294p-45, 0x1.5cc344fa10652p-93 },
	{ 0x1.3687a98aacp-2, 0x1.1623671842fp-45, -0x1.0b428fe1f9e43p-94 },
	{ 0x1.4cb5ec93f4p-2, 0x1.3d50980ea513p-42, 0x1.67f0ea083b1c4p-93 },
	{ 0x1.62e42fefa4p-2, -0x1.8432a1b0e264p-44, 0x1.803f2f6af40f3p-93 },
    },
    {
	{ 0x0p+0, 0x0p+0, 0x0p+0 },
	{ 0x1.62e462b4p-10, 0x1.061d003b97318p-42, 0x1.d7faee66a2e1ep-93 },
	{ 0x1.62e44c92p-9, 0x1.95a7bff5e239p-42, -0x1.f7e788a87135p-95 },
	{ 0x1.0a2b1e33p-8, 0x1.2a3a1a65aa3ap-43, -0x1.54599c9605442p-93 },
	{ 0x1.62e4367cp-8, -0x1.4a995b6d9ddcp-45, -0x1.56bb79b254f33p-100 },
	{ 0x1.bb9d449ap-8, 0x1.8a119c42e9bcp-42, -0x1.8ecf7d8d661f1p-93 },
	{ 0x1.0a2b1f19p-7, 0x1.8863771bd10a8p-42, 0x1.e9731de7f0155p-94 },
	{ 0x1.3687ad11p-7, 0x1.e026a347ca1c8p-42, 0x1.fadc62522444dp-97 },
	{ 0x1.62e436f28p-7, 0x1.25b84f71b70b8p-42, -0x1.fcb3f98612d27p-96 },
	{ 0x1.8f40b7b38p-7, -0x1.62a0a4fd4758p-43, 0x1.3cb3c35d9f6a1p-93 },
	{ 0x1.bb9d3abbp-7, -0x1.0ec48f94d786p-42, -0x1.6b47d410e4cc7p-93 },
	{ 0x1.e7f9bb23p-7, 0x1.e4415cbc97ap-43, -0x1.3729fdb677231p-93 },
	{ 0x1.0a2b22478p-6, -0x1.cb73f4505b03p-42, -0x1.1b3b3a3bc370ap-93 },
	{ 0x1.2059691e8p-6, -0x1.abcc3412f264p-43, -0x1.fe6e998e48673p-95 },
	{ 0x1.3687a768p-6, -0x1.43901e5c97a9p-42, 0x1.b54cdd52a5d88p-96 },
	{ 0x1.4cb5eb5d8p-6, -0x1.8f106f00f13b8p-42, -0x1.8f793f5fce148p-93 },
	{ 0x1.62e432b24p-6, -0x1.745af34bb54b8p-42, -0x1.17e3ec05cde7p-97 },
    },
    {
	{ 0x0p+0, 0x0p+0, 0x0p+0 },
	{ 0x1.62e7bp-14, -0x1.868625640a68p-44, -0x1.34bf0db910f65p-93 },
	{ 0x1.62e35f6p-13, -0x1.2ee3d96b696ap-43, 0x1.a2948cd558655p-94 },
	{ 0x1.0a2b4b2p-12, 0x1.53edbcf1165p-47, -0x1.cfc26ccf6d0e4p-97 },
	{ 0x1.62e4be1p-12, 0x1.783e334614p-52, -0x1.04b96da30e63ap-93 },
	{ 0x1.bb9e085p-12, -0x1.60785f20acb2p-43, -0x1.f33369bf7dff1p-96 },
	{ 0x1.0a2b94dp-11, 0x1.fd4b3a273353p-42, -0x1.685a35575eff1p-96 },
	{ 0x1.368810f8p-11, 0x1.7ded26dc813p-47, -0x1.4c4d1abca79bfp-96 },
	{ 0x1.62e47878p-11, 0x1.7d2bee9a1f63p-42, 0x1.860233b7ad13p-93 },
	{ 0x1.8f40cb48p-11, -0x1.af034eaf471cp-42, 0x1.ae748822d57b7p-94 },
	{ 0x1.bb9d094p-11, -0x1.7a223013a20fp-42, -0x1.1e499087075b6p-93 },
	{ 0x1.e7fa32c8p-11, -0x1.b2e67b1b59bdp-43, -0x1.54a41eda30fa6p-93 },
	{ 0x1.0a2b237p-10, -0x1.7ad97ff4ac7ap-44, 0x1.f932da91371ddp-93 },
	{ 0x1.2059a338p-10, -0x1.96422d90df4p-44, -0x1.90800fbbf2ed3p-94 },
	{ 0x1.36879824p-10, 0x1.0f9054001812p-44, 0x1.9567e01e48f9ap-93 },
	{ 0x1.4cb602cp-10, -0x1.0d709a5ec0b5p-43, 0x1.253dfd44635d2p-94 },
	{ 0x1.62e462b4p-10, 0x1.061d003b97318p-42, 0x1.d7faee66a2e1ep-93 },
    },
    {
	{ 0x0p+0, 0x0p+0, 0x0p+0 },
	{ 0x1.63007cp-18, -0x1.db0e38e5aaaap-43, 0x1.259a7b94815b9p-93 },
	{ 0x1.6300f6p-17, 0x1.2b1c75580438p-44, 0x1.78cabba01e3e4p-93 },
	{ 0x1.0a2115p-16, -0x1.5ff223730759p-42, 0x1.8074feacfe49dp-95 },
	{ 0x1.62e1ecp-16, -0x1.85d6f6487ce4p-45, 0x1.05485074b9276p-93 },
	{ 0x1.bba301p-16, -0x1.af5d58a7c921p-43, -0x1.30a8c0fd2ff5fp-93 },
	{ 0x1.0a32298p-15, 0x1.590faa0883bdp-43, 0x1.95e9bda999947p-93 },
	{ 0x1.3682f1p-15, 0x1.f0224376efaf8p-42, -0x1.5843c0db50d1p-93 },
	{ 0x1.62e3d8p-15, -0x1.142c13daed4ap-43, 0x1.c68a61183ce87p-93 },
	{ 0x1.8f44dd8p-15, -0x1.aa489f399931p-43, 0x1.11c5c376854eap-94 },
	{ 0x1.bb9601p-15, 0x1.9904d8b6a3638p-42, 0x1.8c89554493c8fp-93 },
	{ 0x1.e7f744p-15, 0x1.5785ddbe7cba8p-42, 0x1.e7ff3cde7d70cp-94 },
	{ 0x1.0a2c53p-14, -0x1.6d9e8780d0d5p-43, 0x1.ad9c178106693p-94 },
	{ 0x1.205d134p-14, -0x1.214a2e893fccp-43, 0x1.548a9500c9822p-93 },
	{ 0x1.3685e28p-14, 0x1.e23588646103p-43, 0x1.2a97b26da2d88p-94 },
	{ 0x1.4cb6c18p-14, 0x1.2b7cfcea9e0d8p-42, -0x1.5095048a6b824p-93 },
	{ 0x1.62e7bp-14, -0x1.868625640a68p-44, -0x1.34bf0db910f65p-93 },
    },
  };
  static const double ch[][2] = {
    { 0x1p-1, 0x1.24b67ee516e3bp-111 },
    { -0x1p-2, -0x1.932ce43199a8dp-110 },
    { 0x1.5555555555555p-3, 0x1.55540c15cf91fp-57 },
  };
  static const double cl[3]
      = { -0x1p-3, 0x1.9999999a0754fp-4, -0x1.55555555c3157p-4 };
  uint64_t t = asuint64 (zh);

  int ex = t >> 52, e = ex - 0x3ff + (zl == 0.0);
  t &= ~UINT64_C (0) >> 12;
  t |= UINT64_C (0x3ff) << 52;
  double ed = e;
  uint64_t v = asuint64 (a - ed + 0x1.00008p+0);
  uint64_t i = (v - (UINT64_C (0x3ff) << 52)) >> (52 - 16);
  int i1 = (i >> 12) & 0x1f, i2 = (i >> 8) & 0xf, i3 = (i >> 4) & 0xf,
      i4 = i & 0xf;
  const double l20 = 0x1.62e42fefa38p-2, l21 = 0x1.ef35793c768p-46,
	       l22 = -0x1.9ff0342542fc3p-91;
  double el2 = l22 * ed, el1 = l21 * ed, el0 = l20 * ed;
  double L[3];
  L[0] = LL[0][i1][0] + LL[1][i2][0] + (LL[2][i3][0] + LL[3][i4][0]);
  L[1] = LL[0][i1][1] + LL[1][i2][1] + (LL[2][i3][1] + LL[3][i4][1]);
  L[2] = LL[0][i1][2] + LL[1][i2][2] + (LL[2][i3][2] + LL[3][i4][2]);
  L[0] += el0;
  double t12 = t1[i1] * t2[i2], t34 = t3[i3] * t4[i4];
  double th = t12 * t34, tl = fma (t12, t34, -th);
  double tf = asdouble (t);
  double dh = th * tf, dl = fma (th, tf, -dh);
  double sh = tl * tf, sl = fma (tl, tf, -sh);
  double xl, xh = fasttwosum (dh - 1, dl, &xl);
  if (zl != 0.0)
    {
      t = asuint64 (zl);
      t -= (int64_t) e << 52;
      xl += th * asdouble (t);
    }
  xh = adddd (xh, xl, sh, sl, &xl);
  sl = xh * (cl[0] + xh * (cl[1] + xh * cl[2]));
  sh = polydd (xh, xl, 3, ch, &sl);
  sh = muldd_acc (xh, xl, sh, sl, &sl);
  sh = adddd (sh, sl, el1, el2, &sl);
  sh = adddd (sh, sl, L[1], L[2], &sl);
  double v2, v0 = fasttwosum (L[0], sh, &v2);
  double v1 = fasttwosum (v2, sl, &v2);
  v0 = fasttwosum (v0, v1, &v1);
  v1 = fasttwosum (v1, v2, &v2);
  v0 *= copysign (2, x);
  v1 *= copysign (2, x);
  v2 *= copysign (2, x);
  t = asuint64 (v1);
  if (__glibc_unlikely (!(t & (~UINT64_C (0) >> 12))))
    {
      uint64_t w = asuint64 (v2);
      if ((w ^ t) >> 63)
	t--;
      else
	t++;
      v1 = asdouble (t);
    }
  uint64_t t0 = asuint64 (v0);
  uint64_t er = ((t + 41) & (~UINT64_C (0) >> 12)),
	   de = ((t0 >> 52) & 0x7ff) - ((t >> 52) & 0x7ff);
  double res = v0 + v1;
  if (__glibc_unlikely (de > 99 || er < 80))
    return as_asinh_database (x, res);
  return res;
}
