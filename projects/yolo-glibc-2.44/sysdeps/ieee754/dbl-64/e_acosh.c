/* Correctly-rounded inverse hyperbolic cosine function for the
   binary64 floating point format.

Copyright (c) 2023-2026 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary64/acosh/acosh.c, revision 887cab6f).

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
SOFTWARE. */

/* References:
   [1] Tight and rigourous error bounds for basic building blocks of
       double-word arithmetic, by Mioara Joldeş, Jean-Michel Muller,
       and Valentina Popescu, ACM Transactions on Mathematical Software,
       44(2), 2017.
*/

#include <array_length.h>
#include <stdint.h>
#include <math.h>
#include <libm-alias-finite.h>
#include "math_config.h"
#include "s_asincosh_data.h"
#include "ddcoremath.h"

static double __attribute__ ((noinline)) as_acosh_refine (double, double);
static double __attribute__ ((noinline))
as_acosh_one (double x, double sh, double sl)
{
  static const double ch[][2]
      = { { -0x1.5555555555555p-4, -0x1.5555555554af1p-58 },
	  { 0x1.3333333333333p-6, 0x1.9999998933f0ep-61 },
	  { -0x1.6db6db6db6db7p-8, 0x1.24929b16ec6b7p-63 },
	  { 0x1.f1c71c71c71c7p-10, 0x1.c56d45e265e2cp-66 },
	  { -0x1.6e8ba2e8ba2e9p-11, 0x1.6d50ce7188d3dp-65 },
	  { 0x1.1c4ec4ec4ec43p-12, 0x1.c6791d1cf399ap-66 },
	  { -0x1.c99999999914fp-14, 0x1.ee0d9408a2e2ap-68 },
	  { 0x1.7a878787648e2p-15, -0x1.1cea281e08012p-69 },
	  { -0x1.3fde50d0cb4b9p-16, 0x1.0335101403d9dp-72 },
	  { 0x1.12ef3bf8a0a74p-17, 0x1.f9c6b51787043p-80 } };

  static const double cl[]
      = { -0x1.df3b9d1296ea9p-19, 0x1.a681d7d2298ebp-20,
	  -0x1.77ead7b1ca449p-21, 0x1.4edd2ddb3721fp-22,
	  -0x1.1bf173531ee23p-23, 0x1.613229230e255p-25 };

  double y2
      = x
	* (cl[0]
	   + x
		 * (cl[1]
		    + x * (cl[2] + x * (cl[3] + x * (cl[4] + x * (cl[5]))))));
  double y1 = polydd3 (x, 0, 10, ch, &y2);
  y1 = mulddd (y1, y2, x, &y2);
  double y0 = fasttwosum (1, y1, &y1);
  y1 += y2;
  y0 = muldd_acc2 (y0, y1, sh, sl, &y1);
  return y0 + y1;
}

double
__ieee754_acosh (double x)
{
  uint64_t ix = asuint64 (x);
  if (__glibc_unlikely (ix >= UINT64_C (0x7ff0000000000000)))
    { // x<0 or NaN/Inf
      uint64_t aix = ix << 1;
      if (ix == UINT64_C (0x7ff0000000000000)
	  || aix > (UINT64_C (0x7ff) << 53))
	return x + x; /* +inf or nan */
      return __math_invalid (x);
    }

  if (__glibc_unlikely (ix <= UINT64_C (0x3ff0000000000000)))
    {
      if (ix == UINT64_C (0x3ff0000000000000))
	return 0;
      return __math_invalid (x);
    }
  // now x > 1
  double g, eps;
  int off = 0x3fe;
  uint64_t t = ix;
  if (ix < UINT64_C (0x3ff1e83e425aee63)) // 1 < x < 0x1.1e83e425aee63p+0
    {
      /* this branch was checked exhaustively, with and without FMA
	 contraction */
      double z = x - 1;
      double iz = (-0.25) / z, zt = 2 * z;
      double sh = sqrt (zt),
	     sl = fma (sh, sh, -zt) * (sh * iz);
      static const double cl[] = {
	-0x1.5555555555555p-4,	0x1.3333333332f95p-6,	-0x1.6db6db6d5534cp-8,
	0x1.f1c71c1e04356p-10,	-0x1.6e8b8e3e40d58p-11, 0x1.1c4ba825ac4fep-12,
	-0x1.c9045534e6d9ep-14, 0x1.71fedae26a76bp-15,	-0x1.f1f4f8cc65342p-17
      };
      double z2 = z * z, z4 = z2 * z2,
	     ds = fma (
		 sh * z,
		 cl[0]
		     + z
			   * (((cl[1] + z * cl[2]) + z2 * (cl[3] + z * cl[4]))
			      + z4
				    * ((cl[5] + z * cl[6])
				       + z2 * (cl[7] + z * cl[8]))),
		 sl);
      /* fails with eps = ds*0x1.ffp-51 - 0x1p-104*sh, x=0x1.00a800422847ap+0
	 and rndz (both with/without FMA contraction) */
      eps = ds * 0x1.00p-50 - 0x1p-104 * sh;
      double lb = sh + (ds - eps), ub = sh + (ds + eps);
      if (lb == ub)
	return lb;
      return as_acosh_one (z, sh, sl);
    }
  else if (__glibc_likely (ix < UINT64_C (0x405bf00000000000)))
    {
      /* 0x1.1e83e425aee63p+0 <= x < 111.75: this branch was checked
	 exhaustively with/without FMA contraction */
      off = 0x3ff;
      double x2h = x * x, wh = x2h - 1, wl = fma (x, x, -x2h);
      double sh = sqrt (wh), ish = 0.5 / wh,
	     sl = (wl - fma (sh, sh, -wh)) * (sh * ish);
      double tl, th = fasttwosum (x, sh, &tl);
      tl += sl;
      t = asuint64 (th);
      g = tl / th;
      eps = 0x1.81p-63;
    }
  else if (ix < UINT64_C (0x4087100000000000))
    {
      /* 111.75 <= x < 738: this branch was tested exhaustively
	 with/without FMA contraction */
      static const double cl[]
	  = { 0x1.5c4b6148816e2p-66, -0x1.000000000005cp-2,
	      -0x1.7fffffebf3e6cp-4, -0x1.aab6691f2bae7p-5 };
      double z = 1 / (x * x);
      g = cl[0] + z * (cl[1] + z * (cl[2] + z * cl[3]));
      eps = 0x1.c3p-63;
    }
  else if (ix < UINT64_C (0x40e0100000000000))
    {
      /* 738 <= x < 32896: this branch was tested exhaustively
	 with/without FMA contraction */
      static const double cl[]
	  = { -0x1.7f77c8429c6c6p-67, -0x1.ffffffffff214p-3,
	      -0x1.8000268641bfep-4 };
      double z = 1 / (x * x);
      g = cl[0] + z * (cl[1] + z * cl[2]);
      eps = 0x1.9ap-63;
    }
  else if (ix < UINT64_C (0x41ea000000000000))
    {
      // 32896 <= x < 0x1.ap+31
      /* this branch was tested exhaustively with/without FMA contraction
	 only for 32896 <= x < 2^16. */
      static const double cl[]
	  = { 0x1.7a0ed2effdd1p-67, -0x1.000000017d048p-2 };
      double z = 1 / (x * x);
      g = cl[0] + z * cl[1];
      eps = 0x1.99p-63;
    }
  else
    {
      // 0x1.ap+31 <= x
      g = 0;
      eps = 0x1.b2p-63;
    }
  int ex = t >> 52, e = ex - off;
  t &= ~UINT64_C (0) >> 12; // zero out the exponent field
  double ed = e; // 2*x = (1+t/2^52)*2^ed
  // upper 5 bits of the significand (without leading 1)
  uint64_t i = t >> (52 - 5);
  // low 47 bits of the significand
  int64_t d = t & (~UINT64_C (0) >> 17);
  uint64_t j
      = (t + ((uint64_t) B[i].c0 << 33) + ((int64_t) B[i].c1 * (d >> 16)))
	>> (52 - 10);
  t |= UINT64_C (0x3ff) << 52;
  int i1 = j >> 5, i2 = j & 0x1f;
  double r = r1[i1] * r2[i2], dx = fma (r, asdouble(t), -1), dx2 = dx * dx;
  double f
      = dx2 * ((c[0] + dx * c[1]) + dx2 * ((c[2] + dx * c[3]) + dx2 * c[4]));
  /* l2h+l2l is a double-double approximation of log(2), with error less
     than 2^-102.018, and l2h representable on 42 bits, so that l2h*ed
     is exact */
  const double l2h = 0x1.62e42fefa38p-1, l2l = 0x1.ef35793c7673p-45;
  double lh = (l1[i1][1] + l2[i2][1]) + l2h * ed;
  double t1 = (l2l * ed) + (l1[i1][0] + l2[i2][0]);
  double t2 = f + t1;
  double t3 = g + t2;
  double ll = dx + t3;
  double lb = lh + (ll - eps), ub = lh + (ll + eps);
  if (__glibc_likely (lb == ub))
    return lb;
  // 0x1.71547652b82fep+0 approximates 1/log(2)
  return as_acosh_refine (x, 0x1.71547652b82fep+0 * lb);
}
libm_alias_finite (__ieee754_acosh, __acosh)

static __attribute__ ((noinline)) double
as_acosh_database (double x, double f)
{
  // exceptional values sorted by increasing value
  static const double db[][3] = {
    { 0x1.5bff041b260fep+0, 0x1.a6031cd5f93bap-1, 0x1p-55 },
    { 0x1.9efdca62b700ap+0, 0x1.104b648f113a1p+0, 0x1p-54 },
    { 0x1.a5bf3acfde4b2p+0, 0x1.1585720f35cd9p+0, -0x1p-54 },
    { 0x1.45ea160ddc71fp+7, 0x1.725811dcf6782p+2, 0x1p-52 },
    { 0x1.2a686e4b567cep+10, 0x1.f1c928e7f1e65p+2, 0x1p-52 },
    { 0x1.cb62eec26bd78p+15, 0x1.759a2ad4c4d56p+3, 0x1p-51 },
    { 0x1.3bf8009648dcp+16, 0x1.7fce95ea5c653p+3, -0x1p-53 },
  };
  int a = 0, b = array_length (db) - 1, m = (a + b) / 2;
  while (a <= b)
    { /* binary search */
      if (db[m][0] < x)
	a = m + 1;
      else if (db[m][0] == x)
	{
	  f = db[m][1] + db[m][2];
	  break;
	}
      else
	b = m - 1;
      m = (a + b) / 2;
    }
  return f;
}

// a is an approximation of acosh(x)/log(2)
static double
as_acosh_refine (double x, double a)
{
  // for 0 <= i <= 16, t1[i] is a 26-bit approximation of 2^(-i/2^4)
  static const double t1[]
      = { 0x1p+0,	  0x1.ea4afap-1, 0x1.d5818ep-1,	 0x1.c199bep-1,
	  0x1.ae89f98p-1, 0x1.9c4918p-1, 0x1.8ace54p-1,	 0x1.7a1147p-1,
	  0x1.6a09e68p-1, 0x1.5ab07ep-1, 0x1.4bfdad8p-1, 0x1.3dea65p-1,
	  0x1.306fe08p-1, 0x1.2387a7p-1, 0x1.172b84p-1,	 0x1.0b5587p-1,
	  0x1p-1 };
  // for 0 <= i < 16, t2[i] is a 26-bit approximation of 2^(-i/2^8)
  static const double t2[]
      = { 0x1p+0,	  0x1.fe9d968p-1, 0x1.fd3c228p-1, 0x1.fbdba38p-1,
	  0x1.fa7c18p-1,  0x1.f91d8p-1,	  0x1.f7bfdbp-1,  0x1.f663278p-1,
	  0x1.f507658p-1, 0x1.f3ac948p-1, 0x1.f252b38p-1, 0x1.f0f9c2p-1,
	  0x1.efa1bfp-1,  0x1.ee4aaap-1,  0x1.ecf483p-1,  0x1.eb9f488p-1 };
  // for 0 <= i < 16, t3[i] is a 26-bit approximation of 2^(-i/2^12)
  static const double t3[]
      = { 0x1p+0,	  0x1.ffe9d2p-1,  0x1.ffd3a58p-1, 0x1.ffbd798p-1,
	  0x1.ffa74e8p-1, 0x1.ff91248p-1, 0x1.ff7afb8p-1, 0x1.ff64d38p-1,
	  0x1.ff4eac8p-1, 0x1.ff38868p-1, 0x1.ff22618p-1, 0x1.ff0c3dp-1,
	  0x1.fef61ap-1,  0x1.fedff78p-1, 0x1.fec9d68p-1, 0x1.feb3b6p-1 };
  // for 0 <= i < 16, t4[i] is a 26-bit approximation of 2^(-i/2^16)
  static const double t4[]
      = { 0x1p+0,	  0x1.fffe9dp-1,  0x1.fffd3ap-1,  0x1.fffbd78p-1,
	  0x1.fffa748p-1, 0x1.fff9118p-1, 0x1.fff7ae8p-1, 0x1.fff64cp-1,
	  0x1.fff4e9p-1,  0x1.fff386p-1,  0x1.fff2238p-1, 0x1.fff0c08p-1,
	  0x1.ffef5d8p-1, 0x1.ffedfa8p-1, 0x1.ffec98p-1,  0x1.ffeb35p-1 };
  static const double LL[4][17][3] = {
    /* for 0 <= i <= 16, LL[0][i] is a triple-double approximation
       (h,m,l) of -log(t1[i])/2, where h is a multiple of 2^-40 and |h|<0.347,
       m is a multiple of 2^-91 and |m| < 2^-41, and |l| < 2^-92
    */
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
    /* for 0 <= i < 16, LL[1][i] is a triple-double approximation
       (h,m,l) of -log(t2[i])/2 (LL[1][16] is not used), where h is a
       multiple of 2^-40 and |h| < 0.021, m is a multiple of 2^-91 and
       |m| < 2^-41, and |l| < 2^-92 */
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
    },
    /* for 0 <= i < 16, LL[2][i] is a triple-double approximation
       (h,m,l) of -log(t3[i])/2 (LL[2][16] is not used), where h is a
       multiple of 2^-40 and |h| < 0.002, m is a multiple of 2^-91 and
       |m| < 2^-41, and |l| < 2^-92 */
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
    },
    /* for 0 <= i < 16, LL[3][i] is a triple-double approximation
       (h,m,l) of -log(t4[i])/2 (LL[3][16] is not used), where h is a
       multiple of 2^-40 and |h| < 0.001, m is a multiple of 2^-91 and
       |m| < 2^-41, and |l| < 2^-92 */
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
    },
  };
  static const double ch[][2] = {
    { 0x1p-1, 0x1.24b67ee516e3bp-111 },
    { -0x1p-2, -0x1.932ce43199a8dp-110 },
    { 0x1.5555555555555p-3, 0x1.55540c15cf91fp-57 },
  };
  static const double cl[3]
      = { -0x1p-3, 0x1.9999999a0754fp-4, -0x1.55555555c3157p-4 };
  uint64_t ix = asuint64 (x);
  double zh, zl;
  int huge = 0; // exponent adjustment
  if (ix < UINT64_C (0x4190000000000000)) // x < 2^26
    {
      double x2h = x * x, x2l = fma (x, x, -x2h);
      double wl, wh = x2h - 1;
      wh = fasttwosum (wh, x2l, &wl);
      double sh = sqrt (wh),
	     sl = (wl - fma (sh, sh, -wh)) / (2.0 * sh);
      zh = fasttwosum (x, sh, &zl);
      zl += sl;
      zh = fasttwosum (zh, zl, &zl);
    }
  else if (ix < UINT64_C (0x4330000000000000)) // x < 2^52
    {
      zh = 2 * x;
      zl = -0.5 / x;
    }
  else // x >= 2^52
    {
      /* we don't set zh = 2*x since this will overflow for x > 2^1023,
	 instead we add 1 to e below with zl == 0 */
      zh = x;
      zl = 0;
      huge = 1; // to avoid overflow in zh=2*x
    }
  /* zh+zl is a double-double approximation of x+sqrt(x^2-1),
     or of (x+sqrt(x^2-1))/2 for x >= 2^52 */
  uint64_t t = asuint64 (zh);
  int ex = t >> 52, e = ex - 0x3ff + huge; // 0 <= e <= 1024
  t &= ~UINT64_C (0) >> 12;
  t |= UINT64_C (0x3ff) << 52;
  // now 1 <= t < 2 and x=sqrt(x^2-1) ~ t*2^e
  double ed = e;
  uint64_t v = asuint64 (a - ed + 0x1.00008p+0);
  // since a ~ log2(x+sqrt(x^2-1)) and x+sqrt(x^2-1) ~ 2^e*t
  // we have a ~ e+log2(t) thus a-e ~ log2(t)
  // i = floor(2^16*(v-1))
  uint64_t i = (v - (UINT64_C (0x3ff) << 52)) >> (52 - 16);
  int i1 = (i >> 12) & 0x1f, i2 = (i >> 8) & 0xf, i3 = (i >> 4) & 0xf,
      i4 = i & 0xf;
  const double l20 = 0x1.62e42fefa38p-2, l21 = 0x1.ef35793c768p-46,
	       l22 = -0x1.9ff0342542fc3p-91;
  double el2 = l22 * ed, el1 = l21 * ed, el0 = l20 * ed;
  double L[3];
  /* since all LL[.][.][0] are multiples of 2^-40, |LL[0][i1][0]| < 0.347,
     |LL[1][i2][0]| < 0.021, |LL[2][i3][0]| < 0.002, |LL[3][i4][0]| < 0.001,
     the sum L[0] is exact and less than 0.371 */
  L[0] = LL[0][i1][0] + LL[1][i2][0] + (LL[2][i3][0] + LL[3][i4][0]);
  /* since all LL[.][.][1] are multiples of 2^-91 and less than 2^-41 in
     absolute value, the sum L[1] is exact and less than 2^-39 */
  L[1] = LL[0][i1][1] + LL[1][i2][1] + (LL[2][i3][1] + LL[3][i4][1]);
  /* since all  LL[.][.][2] are less than 2^-92, the sum L[2] is less
     than 2^-90 */
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
      t -= (int64_t) e << 52; // divide by 2^e like t was zh/2^e
      xl += th * asdouble (t);
    }
  xh = adddd2 (xh, xl, sh, sl, &xl);
  sl = xh * (cl[0] + xh * (cl[1] + xh * cl[2]));
  sh = polydd3 (xh, xl, 3, ch, &sl);
  sh = muldd_acc2 (xh, xl, sh, sl, &sl);
  sh = adddd2 (sh, sl, el1, el2, &sl);
  sh = adddd2 (sh, sl, L[1], L[2], &sl);
  double v2, v0 = fasttwosum (L[0], sh, &v2), v1 = fasttwosum (v2, sl, &v2);
  v0 *= 2;
  v1 *= 2;
  v2 *= 2;
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
  uint64_t er = ((t + 7) & (~UINT64_C (0) >> 12)),
	   de = ((t0 >> 52) & 0x7ff) - ((t >> 52) & 0x7ff);
  double res = v0 + v1;
  if (__glibc_unlikely (de > 102 || er < 15))
    return as_acosh_database (x, res);
  return res;
}
