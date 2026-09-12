/* Correctly-rounded natural logarithm of the gamma function for binary64
value.

Copyright (c) 2025 Alexei Sibidanov <sibid@uvic.ca>

The original version of this file was copied from the CORE-MATH
project (file src/binary64/lgamma/lgamma.c, revision 0413bb7e).

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
#include <stdbit.h>
#include <errno.h>
#include <math.h>
#include <libm-alias-finite.h>
#include "math_config.h"
#include "ddcoremath.h"

static double __attribute__ ((noinline)) as_logd (double, double *);
static double __attribute__ ((noinline)) as_logd_accurate (double, double *,
							   double *);
static double __attribute__ ((noinline)) as_sinpipid (double, double *);
static double __attribute__ ((noinline)) as_sinpipid_accurate (double,
							       double *);
static double __attribute__ ((noinline))
as_lgamma_asym_accurate (double, double *, double *);

static __attribute__ ((noinline)) double
as_lgamma_database (double x, double f)
{
  static const double db[][3] = {
    { -0x1.1b649eb4316fbp+3, -0x1.50332a035af1fp+3, -0x1p-51 },
    { -0x1.808d3e2f56b4fp+2, -0x1.d779a9ab6cbffp+0, 0x1p-54 },
    { -0x1.d02b2008d5bf5p+1, -0x1.67cf93db4863ep+0, -0x1p-54 },
    { -0x1.a123d403647d7p+1, -0x1.530824ff0740ap-1, 0x1p-55 },
    { -0x1.90404f46978b6p+1, 0x1.189e211cebce7p-3, -0x1p-57 },
    { -0x1.75692939b3defp+1, 0x1.a11a8b4dd58bbp-1, 0x1p-55 },
    { -0x1.64cc652d46a2dp+1, 0x1.7c2fd07e26d78p-4, -0x1p-58 },
    { -0x1.593ae533139c1p+1, -0x1.316f7a9bb3e4ep-4, 0x1p-58 },
    { -0x1.549cde4f1fd16p+1, -0x1.ab1280c638adap-4, -0x1p-58 },
    { -0x1.53712a3a51156p+1, -0x1.bedf4564c976dp-4, 0x1p-58 },
    { -0x1.51333e5a494f7p+1, -0x1.d93a67f2ad756p-4, 0x1p-58 },
    { -0x1.504f0b5b9d1f9p+1, -0x1.dfa224bc3435ep-4, -0x1p-58 },
    { -0x1.4c213b7fa02dcp+1, -0x1.e051ae181baabp-4, 0x1p-58 },
    { -0x1.75824f4f0c7c8p+0, 0x1.cb23f1fc0296ep-1, 0x1p-55 },
    { -0x1.667cb87bcfa0fp+0, 0x1.f478a478204c5p-1, 0x1p-55 },
    { -0x1.51668b6af122dp+0, 0x1.2728422577ep+0, 0x1p-54 },
    { -0x1.50b41410187c1p+0, 0x1.290131f9d5feap+0, -0x1p-54 },
    { -0x1.307eb80d114afp-2, 0x1.7870d113b3febp+0, -0x1p-54 },
    { -0x1.26923ac1f7c1p-2, 0x1.7df2c32cf08a3p+0, 0x1p-54 },
  };
  int a = 0, b = array_length (db) - 1, m = (a + b) / 2;
  while (a <= b)
    { // binary search
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

static __attribute__ ((noinline)) double
as_lgamma_accurate (double x)
{
  // polynomial coefficients for (lgamma(x)+ln(|x|))/x where x in [-0.25,0.25]
  static const double c0[][2]
      = { { -0x1.2788cfc6fb619p-1, 0x1.6cb90701fbf92p-58 },
	  { 0x1.a51a6625307d3p-1, 0x1.1873d891220dfp-56 },
	  { -0x1.9a4d55beab2d7p-2, 0x1.4c26d1b4d5994p-59 },
	  { 0x1.151322ac7d848p-2, 0x1.b5f9120fdaca6p-57 },
	  { -0x1.a8b9c17aa6149p-3, -0x1.2e826b9f2e3f2p-58 },
	  { 0x1.5b40cb100c306p-3, 0x1.4a79a5f96e16fp-59 },
	  { -0x1.2703a1dcea3aep-3, -0x1.63066fbe425d6p-57 },
	  { 0x1.010b36af86397p-3, -0x1.7438c20a423a8p-59 },
	  { -0x1.c806706d57db4p-4, -0x1.5a891c586f017p-58 },
	  { 0x1.9a01e385d5f8fp-4, 0x1.9eb2a3ca8e809p-59 },
	  { -0x1.748c33114c6d5p-4, -0x1.505ae0736c854p-58 },
	  { 0x1.556ad63243bc2p-4, -0x1.0cfd5c4f86f62p-58 },
	  { -0x1.3b1d971fc59e2p-4, -0x1.9a19a5ba46076p-61 },
	  { 0x1.2496df8320d56p-4, 0x1.51588f44e1564p-58 },
	  { -0x1.11133476e5f97p-4, -0x1.fb27c7d952db7p-59 },
	  { 0x1.00010064c948ap-4, 0x1.c2a9309d327a6p-60 },
	  { -0x1.e1e2d312e9abp-5, 0x1.beee6edc135cfp-59 },
	  { 0x1.c71ce3a414da2p-5, 0x1.7d6e71837bc1dp-62 },
	  { -0x1.af28a18673c39p-5, 0x1.df2c138661f9bp-60 },
	  { 0x1.9999b2dfca9bbp-5, -0x1.afb6a810fa7c2p-59 },
	  { -0x1.861874180c0f2p-5, -0x1.c7900b8893e2bp-59 },
	  { 0x1.745d2798dac6ap-5, 0x1.e962ae5d1c788p-61 },
	  { -0x1.642be2fd1947dp-5, 0x1.94490a4e3d6b4p-61 },
	  { 0x1.55545da95a50bp-5, 0x1.b56c4cc4dd96bp-63 },
	  { -0x1.47ba80f965799p-5, 0x1.3bcac9fdf0d1ap-60 },
	  { 0x1.3b24eb343ccefp-5, -0x1.83fb9b692b013p-61 },
	  { -0x1.2eba18fc9675p-5, -0x1.3069a0cc7e12dp-60 },
	  { 0x1.23b27295dc8b4p-5, 0x1.a6d42b2188101p-60 },
	  { -0x1.2136ba2a595c4p-5, -0x1.c2acae5604f9dp-59 },
	  { 0x1.191f0bce33ba9p-5, -0x1.8470cbefa841ap-61 },
	  { -0x1.b86662fc0fb81p-6, -0x1.236cd67b781c2p-60 },
	  { 0x1.9d6d718c336abp-6, 0x1.c632d9ae925d4p-60 },
	  { -0x1.9ea8d7c263a8p-5, -0x1.bdcf945212655p-59 },
	  { 0x1.9f461fd74cc3p-5, 0x1.5e5f99eb89ba4p-60 } };

  // polynomial coefficients for lgamma(1.5 + x) where x in [-0.25,0.25]
  static const double b[][2]
      = { { -0x1.eeb95b094c191p-4, -0x1.346863f58b074p-58 },
	  { 0x1.2aed059bd608ap-5, 0x1.cd3d2ca77b58cp-63 },
	  { 0x1.de9e64df22ef3p-2, -0x1.6d48ec99346d4p-57 },
	  { -0x1.1ae55b180726cp-3, -0x1.959aeebbdcd5bp-59 },
	  { 0x1.e0f840dad61dap-5, -0x1.599fc3f5e3ccp-59 },
	  { -0x1.da59d5374a543p-6, -0x1.0628c37280b13p-63 },
	  { 0x1.f9ca39daa929cp-7, -0x1.69f468b73daddp-67 },
	  { -0x1.1a8ba4f0ea597p-7, -0x1.7d1edde63f06cp-61 },
	  { 0x1.456f1ad666a3bp-8, -0x1.31d73594fa521p-62 },
	  { -0x1.7edb812f6426ep-9, -0x1.5f45d321e0ae2p-64 },
	  { 0x1.c9735ae9db2c1p-10, -0x1.827b962d41f07p-64 },
	  { -0x1.148a319eec639p-10, 0x1.a64298294f3a5p-64 },
	  { 0x1.517c5a1579f4p-11, -0x1.055841dae38a6p-67 },
	  { -0x1.9eff1d1c8be2dp-12, -0x1.fba56c797e258p-67 },
	  { 0x1.00c41c13e3352p-12, -0x1.9c7d468e4cbd4p-73 },
	  { -0x1.3f6dff22a8ffep-13, 0x1.79e9e76cea415p-68 },
	  { 0x1.8f36195536e0cp-14, -0x1.9de6ad54f6997p-70 },
	  { -0x1.f4ea079eaf734p-15, -0x1.fed2cdd5ce819p-69 },
	  { 0x1.3b5e73a6bf0cep-15, -0x1.b8bf8cedf2f5dp-70 },
	  { -0x1.8e583400720b6p-16, -0x1.58007f65918dcp-70 },
	  { 0x1.f88ed0371a528p-17, 0x1.ca095e5525ba6p-75 },
	  { -0x1.40597e1beb94dp-17, 0x1.99bf6a1da80a1p-71 },
	  { 0x1.97b2f3649dc6ap-18, 0x1.39d919c548f05p-72 },
	  { -0x1.03fa905165ffep-18, 0x1.0e61588355356p-72 },
	  { 0x1.4c8df9a3d368p-19, -0x1.7a6689d0b7dd2p-73 },
	  { -0x1.a9bafae87dbd3p-20, 0x1.5777bb7ae251ap-74 },
	  { 0x1.0b2f2a9e11fe8p-20, -0x1.cf209a5b02c67p-75 },
	  { -0x1.567358802c384p-21, 0x1.c44014666f1c9p-75 },
	  { 0x1.1219755496b17p-21, -0x1.b113ac02e4428p-75 },
	  { -0x1.63804ffdcd648p-22, 0x1.ba1993b4ec9b7p-76 } };

  double sx = x, fh, fl = 0, fll = 0;
  x = fabs (x);
  if (x < 0x1p-100)
    {
      double lll, ll, lh = as_logd_accurate (x, &ll, &lll);
      fh = -lh;
      fl = -ll;
      fll = -lll;
      fh = fasttwosum (fh, fl, &fl);
      fl = fasttwosum (fl, fll, &fll);
      double e;
      fasttwosum (fh, 2 * fl, &e);
      if (e == 0 && 1 + 0x1p-54 == 1 - 0x1p-54)
	fl *= 1 + copysign (0x1p-52, fl) * copysign (1, fll);
    }
  else if (x < 0x1p-2)
    {
      fh = polydddfst (sx, array_length (c0), c0, &fl);
      fh = mulddd2 (sx, fh, fl, &fl);
      double lll, ll, lh = as_logd_accurate (x, &ll, &lll);
      fh = sumdd (fh, fl, -ll, -lll, &fl);
      fh = twosum (-lh, fh, &fll);
      fl = twosum (fll, fl, &fll);
      double e;
      fasttwosum (fh, 2 * fl, &e);
      if (e == 0 && 1 + 0x1p-54 == 1 - 0x1p-54)
	fl *= 1 + copysign (0x1p-52, fl) * copysign (1, fll);
    }
  else
    {
      if (fabs (x - 0.5) < 0x1p-2)
	{
	  fh = polydddfst (x - 0.5, array_length(b), b, &fl);
	  if (sx > 0)
	    {
	      double lll, ll, lh = as_logd_accurate (x, &ll, &lll);
	      fl = sumdd (fl, 0, -ll, -lll, &fll);
	      fh = twosum (fh, -lh, &lh);
	      fl = sumdd (fl, fll, lh, 0, &fll);
	    }
	}
      else if (fabs (x - 2.5) < 0x1p-2)
	{
	  fh = polydddfst (x - 2.5, array_length (b), b, &fl);
	  double lll, ll, lh = as_logd_accurate (x - 1, &ll, &lll);
	  fl = sumdd (fl, 0, ll, lll, &fll);
	  fh = twosum (fh, lh, &lh);
	  fl = sumdd (fl, fll, lh, 0, &fll);
	  if (sx < 0)
	    {
	      lh = as_logd_accurate (x, &ll, &lll);
	      fl = sumdd (fl, fll, ll, lll, &fll);
	      fh = twosum (fh, lh, &lh);
	      fl = sumdd (fl, fll, lh, 0, &fll);
	    }
	}
      else if (fabs (x - 3.5) < 0x1p-2)
	{
	  double l2ll, l2l, l2h = as_logd_accurate (x - 2, &l2l, &l2ll);
	  double l1ll, l1l, l1h = as_logd_accurate (x - 1, &l1l, &l1ll);
	  l1l = sumdd (l1l, l1ll, l2l, l2ll, &l1ll);
	  l1h = fasttwosum (l1h, l2h, &l2h);
	  l1l = sumdd (l1l, l1ll, l2h, 0, &l1ll);
	  fh = polydddfst (x - 3.5, array_length (b), b, &fl);
	  fl = sumdd (fl, 0, l1l, l1ll, &fll);
	  fh = twosum (fh, l1h, &l1h);
	  fl = sumdd (fl, fll, l1h, 0, &fll);
	  if (sx < 0)
	    {
	      double lll, ll, lh = as_logd_accurate (x, &ll, &lll);
	      fl = sumdd (fl, fll, ll, lll, &fll);
	      fh = twosum (fh, lh, &lh);
	      fl = sumdd (fl, fll, lh, 0, &fll);
	    }
	}
      else if (fabs (x - 1) < 0x1p-2)
	{
	  fh = polydddfst (x - 1, array_length (c0), c0, &fl);
	  fh = mulddd2 (x - 1, fh, fl, &fl);
	  if (sx < 0)
	    {
	      double lll, ll, lh = as_logd_accurate (x, &ll, &lll);
	      fl = sumdd (fl, 0, ll, lll, &fll);
	      fh = twosum (fh, lh, &lh);
	      fl = sumdd (fl, fll, lh, 0, &fll);
	    }
	}
      else if (fabs (x - 1.5) < 0x1p-2)
	{
	  fh = polydddfst (x - 1.5, array_length (b), b, &fl);
	  if (sx < 0)
	    {
	      double lll, ll, lh = as_logd_accurate (x, &ll, &lll);
	      fl = sumdd (fl, 0, ll, lll, &fll);
	      fh = twosum (fh, lh, &lh);
	      fl = sumdd (fl, fll, lh, 0, &fll);
	    }
	}
      else if (fabs (x - 2) < 0x1p-2)
	{
	  double lll, ll, lh = as_logd_accurate (x - 1, &ll, &lll);
	  fh = polydddfst (x - 2, array_length (c0), c0, &fl);
	  fh = mulddd2 (x - 2, fh, fl, &fl);
	  fl = sumdd (fl, 0, ll, lll, &fll);
	  fh = twosum (fh, lh, &lh);
	  fl = sumdd (fl, fll, lh, 0, &fll);
	  if (sx < 0)
	    {
	      lh = as_logd_accurate (x, &ll, &lll);
	      fl = sumdd (fl, fll, ll, lll, &fll);
	      fh = twosum (fh, lh, &lh);
	      fl = sumdd (fl, fll, lh, 0, &fll);
	    }
	}
      else if (fabs (x - 3) < 0x1p-2)
	{
	  double l2ll, l2l, l2h = as_logd_accurate (x - 2, &l2l, &l2ll);
	  double l1ll, l1l, l1h = as_logd_accurate (x - 1, &l1l, &l1ll);
	  l1l = sumdd (l1l, l1ll, l2l, l2ll, &l1ll);
	  l1h = fasttwosum (l1h, l2h, &l2h);
	  l1l = sumdd (l1l, l1ll, l2h, 0, &l1ll);
	  fh = polydddfst (x - 3, array_length (c0), c0, &fl);
	  fh = mulddd2 (x - 3, fh, fl, &fl);
	  fl = sumdd (fl, 0, l1l, l1ll, &fll);
	  fh = twosum (fh, l1h, &l1h);
	  fl = sumdd (fl, fll, l1h, 0, &fll);
	  if (sx < 0)
	    {
	      double lll, ll, lh = as_logd_accurate (x, &ll, &lll);
	      fl = sumdd (fl, fll, ll, lll, &fll);
	      fh = twosum (fh, lh, &lh);
	      fl = sumdd (fl, fll, lh, 0, &fll);
	    }
	}
      else
	{ // x>3.75
	  if (sx < 0)
	    x = fasttwosum (x, 1, &fl);
	  fh = as_lgamma_asym_accurate (x, &fl, &fll);
	}
      if (sx < 0)
	{
	  double phi = (sx < -0.5) ? sx - floor (sx) : -sx;
	  double sl, sh = as_sinpipid_accurate (phi, &sl);
	  double lll, ll, lh = as_logd_accurate (sh, &ll, &lll);
	  ll += sl / sh + lll;
	  fl = sumdd (fl, fll, ll, 0, &fll);
	  fh = twosum (fh, lh, &lh);
	  fl = sumdd (fl, fll, lh, 0, &fll);
	  fh = -fh;
	  fl = -fl;
	  fll = -fll;
	}
      fh = fasttwosum (fh, fl, &fl);
      fl = fasttwosum (fl, fll, &fll);
      fh = fasttwosum (fh, fl, &fl);
      fl = fasttwosum (fl, fll, &fll);
      double e;
      fasttwosum (fh, 2 * fl, &e);
      if (e == 0)
	{
	  double dfl = 1 + copysign (0x1p-26, fl) * copysign (0x1p-26, fll);
	  fl *= dfl;
	}
    }

#define SX_BND -0x1.4e147ae147ae1p+1 // approximates -2.61 to nearest

  if (fabs (fh) < 0x1.8p-2)
    {
      if (fabs (fh) < 0x1.74p-4 && sx > SX_BND && sx < -2)
	{
	  static const double x0[]
	      = { 0x1.3a7fc9600f86cp+1, 0x1.55f64f98af8dp-55,
		  0x1.c4b0cd201366ap-110 };
	  static const double c[][2]
	      = { { 0x1.83fe966af535fp+0, -0x1.775909a36a68cp-55 },
		  { 0x1.36eebb002f55dp-1, -0x1.8d4b2124a39f8p-55 },
		  { 0x1.694a6058a7858p-6, -0x1.1d8c8b9b4d80ep-61 },
		  { 0x1.1718d7ca09e5bp-6, 0x1.83195b07fd25p-60 },
		  { 0x1.7339fe04b2764p-10, -0x1.48648f4a4bf9ep-64 },
		  { 0x1.8d32f682aa0bdp-11, -0x1.90953dadfba01p-66 },
		  { 0x1.809f04ee6e0fap-14, -0x1.6100a3d177e7cp-68 },
		  { 0x1.48eaa81657361p-15, 0x1.42b86f83f623dp-70 },
		  { 0x1.9297adb2def5ap-18, -0x1.0c295492288fdp-73 },
		  { 0x1.286fb8cbaebb5p-19, -0x1.ebf1f6a584b62p-77 },
		  { 0x1.a92e0a5de4bc9p-22, -0x1.688db240a9a82p-76 },
		  { 0x1.1a9d4d8c6284ep-23, 0x1.08624c7186639p-78 },
		  { 0x1.c4cd2594e91c9p-26, 0x1.a225e59cfef5dp-81 },
		  { 0x1.18737ec8e68aap-27, 0x1.6421ab9e4c553p-82 },
		  { 0x1.e6028795be5c1p-30, -0x1.b4bd2f36bb0ddp-84 },
		  { 0x1.1eacaeb800afdp-31, -0x1.cedc75cbd8cc4p-87 },
		  { 0x1.06bce9e1f6b9p-33, -0x1.0fb524fda64ebp-87 },
		  { 0x1.2bb110a516b79p-35, 0x1.2d7a224941f92p-90 },
		  { 0x1.1e0024589b848p-37, 0x1.1a84fa416703dp-92 },
		  { 0x1.3ec6ebb4ba73p-39, -0x1.eb9cd73c631d9p-94 },
		  { 0x1.3936af34a015p-41, -0x1.83b442df73674p-95 },
		  { 0x1.57d4ece262198p-43, 0x1.20b160178fca3p-97 },
		  { 0x1.5b530d802ffffp-45, 0x1.8bb2ee582dbd1p-104 },
		  { 0x1.7c323d20053dp-47, -0x1.32d5dd28cac13p-103 },
		  { 0x1.6131339e2b76ap-49, 0x1.4422b26549563p-104 },
		  { 0x1.b4f6aaa0d8886p-52, 0x1.796e66ee63a34p-106 } };
	  const double sc = 0x1p+3;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.168p-4 && sx > -3 && sx < SX_BND)
	{
	  static const double x0[]
	      = { 0x1.5fb410a1bd901p+1, -0x1.a19a96d2e6f85p-54,
		  -0x1.140b4ff4b7d6p-108 };
	  static const double c[][2]
	      = { { -0x1.ea12da904b18cp+0, -0x1.220130f99b2cbp-54 },
		  { 0x1.3267f3c265a52p-1, -0x1.1c630ff19db83p-55 },
		  { -0x1.4185ac30c8bf2p-4, 0x1.f161263693e12p-59 },
		  { 0x1.f504accc9f19bp-7, -0x1.eacc0226c7208p-62 },
		  { -0x1.8588458207eacp-9, 0x1.4b51668f3dff4p-63 },
		  { 0x1.4373f7cc709b3p-11, -0x1.2474e20e777aep-66 },
		  { -0x1.12239bdd6c013p-13, 0x1.46df69aa3032cp-69 },
		  { 0x1.dba65e27421c4p-16, 0x1.36bfe12004625p-71 },
		  { -0x1.a2d2504d7e987p-18, 0x1.a6b6dfe9fa6f4p-75 },
		  { 0x1.7581739ee6087p-20, -0x1.23100aff1ab78p-77 },
		  { -0x1.506c65fad6188p-22, -0x1.17d1749eb738fp-77 },
		  { 0x1.318ef724f7814p-24, -0x1.6517edf27abc2p-82 },
		  { -0x1.17767260d9825p-26, -0x1.03e683dfe1d87p-82 },
		  { 0x1.011e34454ade3p-28, -0x1.2a844fd6e7622p-82 },
		  { -0x1.db8b9e6b1e0e1p-31, -0x1.f73094d7d4e4p-85 },
		  { 0x1.b9bab1fca321p-33, -0x1.89b7151db3448p-88 },
		  { -0x1.9bed46f4d0aa2p-35, -0x1.1af984f4b8e4p-90 },
		  { 0x1.81780d4242119p-37, -0x1.7576147eed5dcp-91 },
		  { -0x1.69d3ce15a3f87p-39, 0x1.9823fc1b6be9ep-93 },
		  { 0x1.5494a34ce847p-41, -0x1.0ec96907a37b4p-95 },
		  { -0x1.4160eccd5982ep-43, 0x1.2b4a92d7b0c76p-98 },
		  { 0x1.2fe0a640fb1ap-45, -0x1.b1f41cf9689edp-106 },
		  { -0x1.1ff8659402df5p-47, 0x1.511b4097b0fccp-104 },
		  { 0x1.1357a3e9e1cf1p-49, -0x1.bda0e37acdd97p-106 },
		  { -0x1.0b145c5c5b9abp-51, -0x1.b52b4002dbc0cp-105 },
		  { 0x1.dc1888c7036cap-54, 0x1.2a69b093c9a8bp-109 },
		  { -0x1.060b52b5d6f68p-56, 0x1.d17de397a2b1p-110 } };
	  const double sc = 0x1p+4;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.2d4p-4 && sx > -3.5 && sx < -3)
	{
	  static const double x0[]
	      = { 0x1.9260dbc9e59afp+1, 0x1.f717cd335a7b3p-53,
		  0x1.d32a2a65bfd63p-107 };
	  static const double c[][2]
	      = { { 0x1.f20a65f2fac55p+2, -0x1.1d258e4b0beb5p-53 },
		  { 0x1.9d4d2977150efp-2, 0x1.a04089578ae88p-56 },
		  { 0x1.c1137124d5c5bp-6, 0x1.d6c922d2512d6p-61 },
		  { 0x1.267203d776b0ep-9, -0x1.aa6081d790e05p-63 },
		  { 0x1.99a6337da39ddp-13, 0x1.49aeda9400147p-68 },
		  { 0x1.293c3f78d3bdbp-16, 0x1.ee59e48e8b181p-73 },
		  { 0x1.bb97aa0b71e45p-20, -0x1.592602ea73795p-78 },
		  { 0x1.51ea3345f5349p-23, 0x1.ef9552f50f84ep-79 },
		  { 0x1.057f65c64b21p-26, -0x1.36ca6c4396475p-80 },
		  { 0x1.99c8650e3a4f9p-30, 0x1.7d3b9d2266a1p-84 },
		  { 0x1.44520c3a4f841p-33, 0x1.8c284070d32dfp-89 },
		  { 0x1.02d221961d8b5p-36, -0x1.0d56a1938ace2p-90 },
		  { 0x1.9ffcd97899d22p-40, -0x1.d6040c8c03da8p-95 },
		  { 0x1.50494b6c20faap-43, -0x1.644c84757ab17p-97 },
		  { 0x1.113fe94711cb9p-46, -0x1.f7b0588af7045p-100 },
		  { 0x1.be09d37bab523p-50, 0x1.0f8dbcb173a93p-106 },
		  { 0x1.6d6b80656d502p-53, -0x1.9e5c21b32ebp-110 },
		  { 0x1.2ca70c7ef211ep-56, 0x1.6511b75c60833p-111 },
		  { 0x1.f9264b4df26e4p-60, -0x1.aefcabcc01fd2p-121 },
		  { 0x1.9262efb57925p-63, -0x1.1196c645b5736p-117 } };
	  const double sc = 0x1p+6;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 7;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.efp-5 && sx > -4 && sx < -3.5)
	{
	  static const double x0[]
	      = { 0x1.fa471547c2fe5p+1, 0x1.70d4561291237p-56,
		  -0x1.9e6fadbbc171ap-111 };
	  static const double c[][2]
	      = { { -0x1.4b99d966c5647p+4, 0x1.9cba2450b0003p-50 },
		  { 0x1.f76deae0436bep-1, -0x1.5af99a1af2d4bp-55 },
		  { -0x1.d25359d4b2f38p-5, 0x1.10c02bb27b3e8p-60 },
		  { 0x1.e8f829f141aa5p-9, 0x1.4b3ff24054d7dp-65 },
		  { -0x1.116f7806d26d3p-12, -0x1.a2efbebc9bc45p-68 },
		  { 0x1.3e8f3ab9fc1f4p-16, 0x1.e3a4940cdb9f6p-70 },
		  { -0x1.7dbbe062ffd9ep-20, -0x1.2986222077472p-74 },
		  { 0x1.d2f76de7bd027p-24, -0x1.6c0ccf4d8669cp-78 },
		  { -0x1.2225fe4f84932p-27, 0x1.a38a5afc8eebap-82 },
		  { 0x1.6d12ae1936ba4p-31, -0x1.de21fabe6423p-86 },
		  { -0x1.cffc2a8f6492dp-35, 0x1.64881462031d6p-91 },
		  { 0x1.294e1bdd838bcp-38, 0x1.dcbb67f373f7dp-92 },
		  { -0x1.7fab625b2f2ffp-42, -0x1.e8527e1dc6098p-100 },
		  { 0x1.f211abe57d76p-46, 0x1.d65f8610deeeep-102 },
		  { -0x1.44f2a05f33c9dp-49, 0x1.3fb27bfc54f94p-106 },
		  { 0x1.a9e4622460be6p-53, 0x1.2f5912b9785cp-107 },
		  { -0x1.182776b50f5cfp-56, -0x1.d30d2e8d300a5p-110 },
		  { 0x1.722b65d64ddcp-60, -0x1.0ee84a196d89cp-114 },
		  { -0x1.f31305b4f263ep-64, 0x1.40fbdbf9d128ap-118 },
		  { 0x1.3db9842b31607p-67, 0x1.ee1ac155bab3ap-122 } };
	  const double sc = 0x1p+8;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 7;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1p-3 && sx > -4.5 && sx < -4)
	{
	  static const double x0[]
	      = { 0x1.0284e78599581p+2, -0x1.e78c1e9e43cfep-53,
		  0x1.2ac17bfd6be92p-108 };
	  static const double c[][2]
	      = { { 0x1.aca5cf4921642p+4, 0x1.a46a2e0d8fdfdp-51 },
		  { 0x1.44415cd813f8ep+1, 0x1.afdc2672876f4p-56 },
		  { 0x1.559b11b2a9c7cp-2, 0x1.17b8ada982c18p-57 },
		  { 0x1.96d18e21aebdbp-5, -0x1.c2f2da6bea629p-62 },
		  { 0x1.0261eb5732e4p-7, 0x1.3910ed6591782p-61 },
		  { 0x1.55e3dbf99eb3dp-10, -0x1.e2d04c05513dcp-64 },
		  { 0x1.d14fe49c4e437p-13, -0x1.d3009aa4e3bb9p-67 },
		  { 0x1.433dce282da6ep-15, -0x1.6a33c9bb3cb85p-70 },
		  { 0x1.c8399c7588ccfp-18, 0x1.6b47c3c6f3b7ap-73 },
		  { 0x1.45fbe666d9415p-20, 0x1.9aaef873df6f7p-75 },
		  { 0x1.d68d794caf0cfp-23, -0x1.62f893b4b1c92p-80 },
		  { 0x1.56729dc75d1c6p-25, -0x1.9e28ba9f5e686p-79 },
		  { 0x1.f5ec3352af509p-28, 0x1.63d11f1c09f18p-82 },
		  { 0x1.7205756353fb8p-30, -0x1.e4315fc28841bp-85 },
		  { 0x1.122e7755f935bp-32, 0x1.e94449e4ce532p-90 },
		  { 0x1.982503f30184cp-35, 0x1.db16eb0f7f6e4p-89 },
		  { 0x1.30f8c448c65bcp-37, -0x1.ba5e1bf8c5346p-95 },
		  { 0x1.c957f8be0461cp-40, 0x1.78aa87d33801bp-95 },
		  { 0x1.57fe1f7e3454fp-42, -0x1.61a8ccec45dcap-96 },
		  { 0x1.035fcc6efbe53p-44, 0x1.454e6ac634253p-98 },
		  { 0x1.87856bbe691edp-47, -0x1.92c14fb55274p-105 },
		  { 0x1.2b877971688d2p-49, 0x1.c72aa6bdd8e43p-104 },
		  { 0x1.e30abef69866bp-52, 0x1.4e95354428576p-107 },
		  { 0x1.3f5a80dfa357ep-54, 0x1.dc7c678b87e2ep-108 } };
	  const double sc = 0x1p+7;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 7;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.08p-4 && sx > -5 && sx < -4.5)
	{
	  static const double x0[]
	      = { 0x1.3f7577a6eeafdp+2, -0x1.5de5eab7f12cfp-53,
		  0x1.4075f5e0494a2p-110 };
	  static const double c[][2]
	      = { { -0x1.d224a3ef9e41fp+6, -0x1.9be272a13bcb6p-48 },
		  { 0x1.b533c678a3956p+2, -0x1.37da6a2b62e3cp-53 },
		  { -0x1.0d3f7fee65d34p-1, 0x1.e68bf720db9bep-55 },
		  { 0x1.752a6f5ac2726p-5, -0x1.16f2fc46d2a34p-62 },
		  { -0x1.13d5d163bd3f7p-8, -0x1.813df737e6e21p-62 },
		  { 0x1.a8c5c53458ca5p-12, 0x1.02a507fa1273dp-66 },
		  { -0x1.5068b3ed69409p-15, -0x1.8fc69d833eb31p-72 },
		  { 0x1.0ffa575ea7fe3p-18, 0x1.ab6c92ac78cc4p-75 },
		  { -0x1.bec12dd78a23p-22, 0x1.81392cb5ee3ap-76 },
		  { 0x1.7382570f0b3c5p-25, 0x1.6c165015a71c3p-79 },
		  { -0x1.380ebf6161ccep-28, -0x1.efc3933cb1d2cp-83 },
		  { 0x1.084de43d1a947p-31, -0x1.1fca46f4984d6p-85 },
		  { -0x1.c2d90dead7a34p-35, 0x1.b9c200207ddp-89 },
		  { 0x1.82d0afe1a76ap-38, -0x1.958789fbe4df9p-93 },
		  { -0x1.4d93cbb2d04c5p-41, -0x1.411bc9bf3a7ccp-95 },
		  { 0x1.20ea504f0c629p-44, -0x1.1c1a63ab9daddp-100 },
		  { -0x1.f6cb7ebb79bc2p-48, 0x1.f2f3b682d06fep-106 },
		  { 0x1.be565af661ea4p-51, 0x1.b54b6ecaea8d5p-108 },
		  { -0x1.78510a36bad6cp-54, -0x1.6935e8745afe8p-109 } };
	  const double sc = 0x1p+10;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 7;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.1p-4 && sx > -5.5 && sx < -5)
	{
	  static const double x0[]
	      = { 0x1.4086a57f0b6d9p+2, 0x1.95262b72ca9cap-55,
		  0x1.bd98d5e0861aap-109 };
	  static const double c[][2]
	      = { { 0x1.ed72e0829ae02p+6, -0x1.fdc1859ae9c53p-50 },
		  { 0x1.cecc32ec22f9bp+2, 0x1.b6ecc779b86fcp-53 },
		  { 0x1.253d8563f7264p-1, -0x1.5cd273fbd9f05p-55 },
		  { 0x1.a225df2da6e63p-5, -0x1.fe9d0b1b4c66p-59 },
		  { 0x1.3e01773762671p-8, -0x1.f0e1184c71c78p-62 },
		  { 0x1.f7d8d5bdcb186p-12, -0x1.d3eb6b34618fbp-66 },
		  { 0x1.9a8d00c77a92cp-15, -0x1.107ec223e7938p-70 },
		  { 0x1.557fd8c490b43p-18, 0x1.d8ff4f1780ac1p-72 },
		  { 0x1.209221a624132p-21, -0x1.990b9a98394afp-75 },
		  { 0x1.edc98d3bbeedfp-25, 0x1.58dea3f0bc5b9p-79 },
		  { 0x1.aabd28e6c9a81p-28, -0x1.da694af0e0ffp-82 },
		  { 0x1.73de2dd1cfd61p-31, 0x1.4f223cad85a45p-85 },
		  { 0x1.4651832f82e2ep-34, 0x1.98ad9bf65ef61p-92 },
		  { 0x1.200d84be7e1fcp-37, -0x1.5a13d3eb64f54p-93 },
		  { 0x1.ff278dbfe563p-41, 0x1.7119b25262c2dp-100 },
		  { 0x1.c77e7644c5411p-44, 0x1.c21cc9e02459cp-99 },
		  { 0x1.97c73d128923ap-47, -0x1.155bb925610d9p-102 },
		  { 0x1.74798cd59653bp-50, -0x1.90790da400208p-104 },
		  { 0x1.436d37cbd1ac2p-53, 0x1.34f1a6c134053p-107 } };
	  const double sc = 0x1p+10;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.13p-3 && sx > -6 && sx < -5.5)
	{
	  static const double x0[]
	      = { 0x1.7fe92f591f40dp+2, 0x1.7dd4ed62cbd32p-52,
		  -0x1.2071c071a2146p-108 };
	  static const double c[][2]
	      = { { -0x1.661f6a43a5e12p+9, -0x1.0c437b83bc0ddp-45 },
		  { 0x1.f79dcb794f26fp+5, -0x1.ada8018ade6efp-52 },
		  { -0x1.d6e8088a19ffep+2, -0x1.2c08708631269p-53 },
		  { 0x1.ef5d308dbfc97p-1, 0x1.87cd5c0d349fap-58 },
		  { -0x1.15ea6b0ab529ep-3, 0x1.4ac7004538519p-66 },
		  { 0x1.44d54e9fe2397p-6, 0x1.f1ca84e1a96cp-62 },
		  { -0x1.8684e40cebb3dp-9, -0x1.19803482caec7p-64 },
		  { 0x1.df44c1d81c723p-12, -0x1.0ed2e1ad825e8p-67 },
		  { -0x1.2ac3053f4ee18p-14, -0x1.fe50dd710dd6bp-68 },
		  { 0x1.79226ae04a847p-17, -0x1.379839e0000d6p-75 },
		  { -0x1.e0dffb5f77ccbp-20, 0x1.a4015df3482b4p-74 },
		  { 0x1.35217890708e7p-22, 0x1.2875bd051d55ep-78 },
		  { -0x1.903aa9af5cf03p-25, -0x1.a619f5e70dc34p-83 },
		  { 0x1.04a103323cd6ap-27, 0x1.a260aa452be23p-83 },
		  { -0x1.552efc1ecd295p-30, -0x1.88a83f178c25p-84 },
		  { 0x1.c0a12b600b141p-33, -0x1.c3732154b52acp-89 },
		  { -0x1.281ce75ae916fp-35, -0x1.a96d8e0798081p-89 },
		  { 0x1.88411a65d3af4p-38, -0x1.0e8c83eb281a9p-92 },
		  { -0x1.049e79a4b5baap-40, -0x1.64caa9cfd44a2p-94 },
		  { 0x1.5b0f16f419f7bp-43, 0x1.74e160750d876p-102 },
		  { -0x1.ce70ccad93bc1p-46, 0x1.1b340ff57e754p-100 },
		  { 0x1.3a7c53f75547fp-48, -0x1.88a8d5ad25f3fp-102 },
		  { -0x1.c41c8d3e4c2c9p-51, 0x1.e4dd9e959c88bp-105 },
		  { 0x1.f82da393ec32ap-54, -0x1.65c64b7f7e325p-115 } };
	  const double sc = 0x1p+12;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.15p-3 && sx > -6.5 && sx < -6)
	{
	  static const double x0[]
	      = { 0x1.8016b25897c8dp+2, -0x1.27e0f49a4ba72p-54,
		  0x1.72e1ab15a4d03p-110 };
	  static const double c[][2]
	      = { { 0x1.69de49e3af2aap+9, 0x1.954b690943afcp-47 },
		  { 0x1.fce23484cfd1p+5, 0x1.8266e7580f08cp-49 },
		  { 0x1.de503a3c37c4p+2, 0x1.9fa7459d62427p-53 },
		  { 0x1.f9c7b52558abbp-1, 0x1.b6896749474c1p-55 },
		  { 0x1.1d3d50714416ap-3, 0x1.56020c7693337p-58 },
		  { 0x1.4f21e2fb9e06p-6, 0x1.9e1cfc6a7c13p-60 },
		  { 0x1.9500994cd8a9ep-9, -0x1.ce1777b097bd6p-63 },
		  { 0x1.f3a2c23c19d79p-12, -0x1.972494ba4cd4dp-70 },
		  { 0x1.39152652eb3aap-14, 0x1.8f85c4250ca8p-69 },
		  { 0x1.8d45f8be891dep-17, 0x1.41e12761b265ap-71 },
		  { 0x1.fd3214a702b29p-20, -0x1.140076f8887f9p-74 },
		  { 0x1.490b476815d46p-22, -0x1.3c92eb9376d39p-81 },
		  { 0x1.ac3b965280088p-25, -0x1.468539ffe88b5p-82 },
		  { 0x1.1851c43c40d43p-27, -0x1.6918e4fb64554p-82 },
		  { 0x1.70dfb529f2c5ap-30, -0x1.b5dfdc2a8890dp-84 },
		  { 0x1.e791ef4ef8602p-33, 0x1.b20188eabd9e6p-90 },
		  { 0x1.437e60f850abep-35, 0x1.b5786abafa12ep-89 },
		  { 0x1.aec29bcf6c8c1p-38, 0x1.774aed905f5cfp-92 },
		  { 0x1.1fb22834000f1p-40, 0x1.705349812bcbap-95 },
		  { 0x1.811ce4ec7b869p-43, 0x1.8be5e35bce704p-99 },
		  { 0x1.01e72b00fd763p-45, 0x1.c8c867f1090e3p-99 },
		  { 0x1.60a0a9eee14d8p-48, 0x1.ed307bf965e72p-106 },
		  { 0x1.fdce60428ba59p-51, 0x1.268eaf9e15016p-106 },
		  { 0x1.1dcf4e3ac0033p-53, 0x1.bdbdd6256584p-108 } };
	  const double sc = 0x1p+12;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.34p-3 && sx > -7.0 && sx < -6.5)
	{
	  static const double x0[]
	      = { 0x1.bffcbf76b86fp+2, -0x1.853b29347b806p-57,
		  0x1.0fa018051dd41p-111 };
	  static const double c[][2]
	      = { { -0x1.3abf7a5cea91bp+12, -0x1.8257b8abd02cep-42 },
		  { 0x1.8349a2550422dp+9, -0x1.c6f2ef4105b99p-45 },
		  { -0x1.3d91dadc98428p+7, 0x1.46605ff1fc41cp-48 },
		  { 0x1.24f3d636f3339p+5, 0x1.5966a8447ea56p-49 },
		  { -0x1.20427df1b3492p+3, -0x1.e9801a53bb06ep-52 },
		  { 0x1.2775e857fb69cp+1, 0x1.88bd494f9ea51p-53 },
		  { -0x1.377e70b463c13p-1, -0x1.2524c9cdd93dbp-55 },
		  { 0x1.4f3d28edba5cdp-3, 0x1.92d2ac40dac77p-60 },
		  { -0x1.6e8557168cf8ap-5, 0x1.0b5cf2a57fc71p-59 },
		  { 0x1.95bb17ce427a4p-7, -0x1.0f3a87df6aaa1p-66 },
		  { -0x1.c5ac12d48f7e4p-9, 0x1.1d042aabfd8fbp-63 },
		  { 0x1.ff816dad74e75p-11, 0x1.24594bf64f76dp-67 },
		  { -0x1.225f4a6a0c5b4p-12, -0x1.e1dd93bdafddap-66 },
		  { 0x1.4ba3e5c007209p-14, 0x1.a47689fcd5098p-70 },
		  { -0x1.7cb738040762cp-16, -0x1.f100cc43c4b52p-70 },
		  { 0x1.b701200fca8b8p-18, 0x1.d825b96ec4cebp-72 },
		  { -0x1.fc33bee21552cp-20, 0x1.096b388c1821dp-75 },
		  { 0x1.272cc14fa7a53p-21, 0x1.d22fcb28979f7p-76 },
		  { -0x1.57f61caeb3e48p-23, -0x1.03971054f5156p-79 },
		  { 0x1.91f102889ab68p-25, -0x1.b9949bdf154e2p-80 },
		  { -0x1.d641c300c655ep-27, -0x1.47bf835c77fc5p-82 },
		  { 0x1.1319b4831f0ep-28, -0x1.2688b8ef3d8f7p-84 },
		  { -0x1.4c01654897d29p-30, -0x1.5c8d35e08cb51p-86 },
		  { 0x1.a7ea9eb248f1ep-32, -0x1.346b798e3ef55p-86 },
		  { -0x1.8afe498da415bp-34, 0x1.3f02ddea04afbp-88 } };
	  const double sc = 0x1p+14;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.145p-3 && sx > -7.5 && sx < -7)
	{
	  static const double x0[]
	      = { 0x1.c0033fdedfe1fp+2, -0x1.20bb7d2324678p-52,
		  -0x1.f5536678d69d3p-106 };
	  static const double c[][2]
	      = { { 0x1.3b407aa387bd1p+12, 0x1.da1e57343b1dbp-43 },
		  { 0x1.83e85daafbad6p+8, -0x1.f37538d99bd29p-46 },
		  { 0x1.3e552b5e3c226p+5, -0x1.07b1550d11cf4p-49 },
		  { 0x1.25e42a45e905bp+2, 0x1.61a63054c47c5p-54 },
		  { 0x1.216a3560743eep-1, 0x1.f5024887fdb9dp-57 },
		  { 0x1.28e1c70ef5313p-4, 0x1.3234af5080105p-59 },
		  { 0x1.393e2bc330081p-7, -0x1.45ef9ada326bap-62 },
		  { 0x1.5164141f5ae6ap-10, -0x1.802404a8fab89p-65 },
		  { 0x1.712b3a86e1bdfp-13, -0x1.b2f4283e8796ep-69 },
		  { 0x1.98fd36b906e08p-16, -0x1.c84ad8d8481b7p-70 },
		  { 0x1.c9ae6ef6262f6p-19, 0x1.ff0ee333033bfp-73 },
		  { 0x1.02382a958b63dp-21, -0x1.99f49c2b45652p-75 },
		  { 0x1.256845ec9b6b1p-24, 0x1.822bd9a29ab32p-80 },
		  { 0x1.4f5ff36087486p-27, -0x1.dc262e92f86f3p-82 },
		  { 0x1.814f9ce48442fp-30, 0x1.544e6fd9ac4bep-84 },
		  { 0x1.bca89f844ac84p-33, -0x1.63ceb4f54c669p-87 },
		  { 0x1.01946c9b7b26ap-35, -0x1.9b4cb6a88b6fcp-90 },
		  { 0x1.2b75936f37bc3p-38, -0x1.d8809500cd622p-94 },
		  { 0x1.5d3d0a248647p-41, -0x1.061c56c047c1dp-95 },
		  { 0x1.98297f709854fp-44, 0x1.4cc4272ef2e04p-103 },
		  { 0x1.dd4cc8e9d77b5p-47, 0x1.0103a0db6c049p-102 },
		  { 0x1.1ce46c495fa62p-49, 0x1.622087cd95f2ap-103 },
		  { 0x1.678ec65743609p-52, -0x1.850d52cb53a9fp-107 },
		  { 0x1.5fe290153bcaap-55, 0x1.a8b6414621298p-109 } };
	  const double sc = 0x1p+15;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.76bp-3 && sx > -8 && sx < -7.5)
	{
	  static const double x0[]
	      = { 0x1.ffff97f8159cfp+2, 0x1.e54f415a91586p-55,
		  0x1.53a5d106f9a3ep-109 };
	  static const double c[][2]
	      = { { -0x1.3af76fe4c2fabp+15, -0x1.7cc92f0b999ep-40 },
		  { 0x1.838e76caaf123p+12, 0x1.292e15f502d1fp-42 },
		  { -0x1.3de68b3256526p+10, 0x1.5456a490ea6e3p-44 },
		  { 0x1.255c052530c71p+8, -0x1.6700425996a73p-48 },
		  { -0x1.20c2a8418126ap+6, 0x1.1d5e3d82023c1p-50 },
		  { 0x1.28139342cefp+4, 0x1.0238cfdd7475dp-51 },
		  { -0x1.384066c322246p+2, 0x1.ce82f225b48d1p-53 },
		  { 0x1.502bc4dad47d3p+0, 0x1.f45416fab08dap-58 },
		  { -0x1.6faadfece0e3p-2, -0x1.6c5874c40cd48p-56 },
		  { 0x1.9724323c89909p-4, -0x1.27460acd35293p-58 },
		  { -0x1.c7684c96f243p-6, 0x1.5d749ebbd28fep-62 },
		  { 0x1.00d1f48743fb8p-7, 0x1.ad60125333897p-61 },
		  { -0x1.23af6dd470e5cp-9, -0x1.1d084246f0edap-64 },
		  { 0x1.4d416958eec41p-11, -0x1.dfeda2a7c037ep-67 },
		  { -0x1.7eb3eb405269fp-13, -0x1.95169511f695dp-69 },
		  { 0x1.b972ec5a016fp-15, 0x1.0a9a114630056p-69 },
		  { -0x1.ff35af33eaacp-17, -0x1.73922358a5b41p-71 },
		  { 0x1.290662f9abea1p-18, -0x1.8a5267d544bb7p-72 },
		  { -0x1.5a395633f9d69p-20, -0x1.377120c488d2ep-76 },
		  { 0x1.94b2a8f6b566ep-22, 0x1.685bd8f7318c7p-79 },
		  { -0x1.da517a6c7ab89p-24, -0x1.940f32a9a6413p-78 },
		  { 0x1.168992efa74a9p-25, -0x1.bb9b7bf492795p-81 },
		  { -0x1.4665c665b014ep-27, 0x1.91ef4857aca34p-81 },
		  { 0x1.7f49e136d0e91p-29, 0x1.9d7da1584fe9p-85 },
		  { -0x1.df48e4db635efp-31, -0x1.ac6dd06d9a112p-85 },
		  { 0x1.3665697708ddp-32, -0x1.e15f660c03e5bp-90 },
		  { -0x1.015f65348c21ap-34, -0x1.e45b91e0267cp-90 } };
	  const double sc = 0x1p+17;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.76cp-3 && sx > -0x1.10f5c28f5c28fp+3
	       && sx < -8)
	{
	  static const double x0[]
	      = { 0x1.000034028b3f9p+3, 0x1.f60cb3cec1cedp-52,
		  -0x1.ea26620d6b1cap-106 };
	  static const double c[][2]
	      = { { 0x1.3b088fed67718p+15, -0x1.505613ba29893p-39 },
		  { 0x1.83a3893550edcp+12, 0x1.f52e3b240db3p-42 },
		  { 0x1.3e0078db8ada4p+10, 0x1.506573eecc6e3p-44 },
		  { 0x1.257bec9464251p+8, 0x1.8c4ea8394aa1ep-49 },
		  { 0x1.20e9ea0755a47p+6, -0x1.978e59e1a3d9fp-48 },
		  { 0x1.2843e1313c83bp+4, -0x1.50610d6737717p-55 },
		  { 0x1.387bd6a785478p+2, -0x1.1f1fff3fa1b4bp-52 },
		  { 0x1.5074e788de77p+0, 0x1.aeeb83580ea25p-55 },
		  { 0x1.7004dd990d7dap-2, 0x1.3f7f554ffc67p-59 },
		  { 0x1.9792ed5f6dfb4p-4, -0x1.1f23ec3a02887p-58 },
		  { 0x1.c7f08cdaef32fp-6, -0x1.1e18e4c767dep-62 },
		  { 0x1.0125c81121f59p-7, -0x1.a6731270f4d3fp-61 },
		  { 0x1.2416931f22d22p-9, -0x1.2bc2867243e45p-64 },
		  { 0x1.4dc0543bea659p-11, 0x1.635ca3a9b1a8dp-66 },
		  { 0x1.7f501645b2fdep-13, -0x1.e45b9db4c768p-67 },
		  { 0x1.ba331549d971dp-15, 0x1.97379411be4a6p-69 },
		  { 0x1.001110caa210fp-16, 0x1.68ce3f9f57bc4p-70 },
		  { 0x1.2997db55cba74p-18, -0x1.0f0fa5ac41b0ap-74 },
		  { 0x1.5aec550050044p-20, 0x1.7fdca933bee56p-74 },
		  { 0x1.958ee8d69504p-22, 0x1.c28e192bc3ba4p-76 },
		  { 0x1.db608c6ccccc8p-24, -0x1.71a440ef759bep-80 },
		  { 0x1.173059c7e38cfp-25, 0x1.40d18009e343fp-79 },
		  { 0x1.4731fe682342cp-27, -0x1.535cdcf9e608bp-83 },
		  { 0x1.8043ec54aba15p-29, -0x1.62d6d36f1d19ep-84 },
		  { 0x1.e08fc2d5d4ap-31, -0x1.6e4b1587c874ep-85 },
		  { 0x1.3742f9e468b63p-32, 0x1.4c5c5de4449bfp-88 },
		  { 0x1.021df5d9d312dp-34, 0x1.b2da076b6955fp-90 } };
	  const double sc = 0x1p+17;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.99p-3 && sx > -9 && sx < -8.5)
	{
	  static const double x0[]
	      = { 0x1.1ffffa3884bdp+3, 0x1.ff90c9d2ae925p-53,
		  -0x1.30c0efef78c04p-107 };
	  static const double c[][2]
	      = { { -0x1.625edfc63db2fp+18, 0x1.da7fc3ed68f99p-37 },
		  { 0x1.ea8c150480a7ap+15, 0x1.344e4cbf6d2a1p-39 },
		  { -0x1.c4b30e4bc55c1p+13, -0x1.9ec40fdd810d7p-41 },
		  { 0x1.d5fe468dbbf03p+11, -0x1.80705f1856688p-43 },
		  { -0x1.043d21bc24decp+10, -0x1.b0db95c85f28bp-45 },
		  { 0x1.2c334ae535e1dp+8, 0x1.530b0b83f8e2p-46 },
		  { -0x1.64314b431cd64p+6, -0x1.5d7e4374b5c05p-49 },
		  { 0x1.af6ed589b3a86p+4, -0x1.622e836ea2287p-51 },
		  { -0x1.096e446edcfb4p+3, 0x1.f849c504338f9p-51 },
		  { 0x1.4aaf49e713c0cp+1, 0x1.bbab832e792a1p-53 },
		  { -0x1.a0246d9c1b57fp-1, -0x1.18f4c5d12b8b5p-56 },
		  { 0x1.0806315c1a365p-2, 0x1.074a0bafc599fp-58 },
		  { -0x1.515dd6b891094p-4, -0x1.bcca3edb6dc58p-59 },
		  { 0x1.b1a5fe76de3d4p-6, -0x1.f8d7b8e44c315p-63 },
		  { -0x1.182229539b98cp-7, 0x1.5c2ae71e11bfep-62 },
		  { 0x1.6b8b31630a98cp-9, -0x1.c5c500af742dap-65 },
		  { -0x1.d9a3c6789b8e8p-11, 0x1.7160288934ba5p-65 },
		  { 0x1.359c2ea889b3ep-12, 0x1.e44b62289b60bp-69 },
		  { -0x1.9606a9f683511p-14, -0x1.3a3c7889c7af5p-68 },
		  { 0x1.0af84fb66238ep-15, -0x1.3d487a70215dap-69 },
		  { -0x1.5ff88107d908ap-17, 0x1.0c4172ceada86p-73 },
		  { 0x1.d14070a537453p-19, 0x1.964ddb3dba2bap-73 },
		  { -0x1.33eaa64e4654ep-20, 0x1.e6deb19e51b4ep-74 },
		  { 0x1.957ef375ea2abp-22, 0x1.065699585b36cp-77 },
		  { -0x1.0cd5c9d34a242p-23, -0x1.6f2ed041b79a1p-77 },
		  { 0x1.836d31552e5d6p-25, -0x1.789b28b32b093p-79 },
		  { -0x1.19aeee4b34ef6p-26, -0x1.c45d882aaaep-81 },
		  { 0x1.eb1912eaced35p-29, 0x1.e9842f288e554p-84 } };
	  const double sc = 0x1p+20;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.99p-3 && sx > -9.5 && sx < -9)
	{
	  static const double x0[]
	      = { 0x1.200005c7768fbp+3, 0x1.b5b610ffb70d4p-54,
		  0x1.deb7ad09ec5eap-108 };
	  static const double c[][2]
	      = { { 0x1.626120391944p+18, 0x1.7d5e8272ce41dp-38 },
		  { 0x1.ea8f32fb7f586p+15, -0x1.345b1cc20d4a5p-39 },
		  { 0x1.c4b75ee68e2bap+13, -0x1.812d7bcedccb5p-42 },
		  { 0x1.d6043fa1ffaa5p+11, -0x1.5a4eaff5f008ap-43 },
		  { 0x1.04414411db7f4p+10, 0x1.d742c54011402p-44 },
		  { 0x1.2c3903ec9c90cp+8, 0x1.73da27a4b1b2p-46 },
		  { 0x1.64393744bb9bdp+6, -0x1.482bcb016f26fp-48 },
		  { 0x1.af79ccdc71d33p+4, 0x1.0a7a439456745p-50 },
		  { 0x1.0975db7d71fc6p+3, 0x1.bd477ffdd39f6p-51 },
		  { 0x1.4ab9cba1e3478p+1, 0x1.1c929f5a50618p-54 },
		  { 0x1.a032f8f114635p-1, 0x1.92279b6d57eb2p-55 },
		  { 0x1.0810426bfa5a1p-2, 0x1.0cd3cd3381e06p-56 },
		  { 0x1.516bc616eaf53p-4, -0x1.0ebc405b4871ap-58 },
		  { 0x1.b1b948b11a02dp-6, 0x1.8b0ffc5af8ce1p-60 },
		  { 0x1.182f8343c9e61p-7, -0x1.de95c6fadf8bbp-61 },
		  { 0x1.6b9dacc2e40dp-9, 0x1.7d2afb82825d1p-63 },
		  { 0x1.d9bd5c016a05ap-11, 0x1.afdd41ec7d841p-65 },
		  { 0x1.35ade3d85cd0ep-12, -0x1.5080702a55ab7p-66 },
		  { 0x1.961f2d2659b02p-14, 0x1.f377d9fd7df3ap-68 },
		  { 0x1.0b0946f97b203p-15, 0x1.9ecb24ffa7674p-69 },
		  { 0x1.600ffd56f5733p-17, 0x1.2ffa0ff103bfdp-71 },
		  { 0x1.d160f5b944255p-19, -0x1.6f04901227877p-75 },
		  { 0x1.3401289d10771p-20, -0x1.b2fca86443a23p-76 },
		  { 0x1.959dec69f88edp-22, 0x1.1945258ad2d8p-76 },
		  { 0x1.0ceb1d5fdcfdbp-23, -0x1.e1bf43eb8d61fp-77 },
		  { 0x1.838cdbbcfd467p-25, -0x1.948ada050fa52p-80 },
		  { 0x1.19c71e9accac3p-26, 0x1.74cb4459f6b9p-81 },
		  { 0x1.eb4678e3fcf16p-29, -0x1.66454030f804fp-83 } };
	  const double sc = 0x1p+20;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.76bp-3 && sx > -10 && sx < -9.5)
	{
	  static const double x0[]
	      = { 0x1.3fffff6c0d7cp+3, -0x1.197cea8c42d7dp-51,
		  -0x1.7072c5a292198p-105 };
	  static const double c[][2]
	      = { { -0x1.baf7da5f3795dp+21, -0x1.16a79518c8367p-33 },
		  { 0x1.7f3e8791fa0d2p+18, -0x1.2aec811c9609ep-36 },
		  { -0x1.ba18befcaaa63p+15, -0x1.d18c4e260465p-39 },
		  { 0x1.1ede14765dc0cp+13, 0x1.13bc952384f0fp-41 },
		  { -0x1.8d1a9ab5a505p+10, -0x1.904ee4f738385p-46 },
		  { 0x1.1e4d8c35d22ccp+8, -0x1.3b56f0fecec54p-48 },
		  { -0x1.a8a191db109p+5, -0x1.11eec467fa814p-51 },
		  { 0x1.4174f65ff868p+3, 0x1.c939d4bfd1404p-51 },
		  { -0x1.ee6d90f2332c7p+0, 0x1.3b18f44783de9p-55 },
		  { 0x1.80fd3420fba09p-2, 0x1.55812b31052fbp-57 },
		  { -0x1.2ecd481762eaep-4, -0x1.d974aa28e0c7p-62 },
		  { 0x1.e04a0b28db4c5p-7, -0x1.f0d1f410ed5c5p-61 },
		  { -0x1.7f91af3f1200fp-9, 0x1.da2dc770582ddp-63 },
		  { 0x1.342652fcfe1b6p-11, -0x1.7744fdcaebcfap-66 },
		  { -0x1.f1a88f796a348p-14, 0x1.a8bb92c660ef4p-74 },
		  { 0x1.93a68769f8a0fp-16, -0x1.f24bb59f0e5dap-70 },
		  { -0x1.48af467f6d621p-18, 0x1.7948f005903cap-74 },
		  { 0x1.0c92181e40e32p-20, 0x1.65951755e7adfp-74 },
		  { -0x1.b842459ce40bfp-23, 0x1.4503ca5bb9422p-77 },
		  { 0x1.69db729e3b047p-25, -0x1.c0423483dea32p-79 },
		  { -0x1.2a3762889a1ebp-27, -0x1.c935e18265a7p-81 },
		  { 0x1.ec8fbfde50a33p-30, 0x1.98aa5e946ac0fp-89 },
		  { -0x1.95dcfae4547bcp-32, -0x1.c04aac4c4ba01p-87 },
		  { 0x1.4f21457a90c36p-34, -0x1.e01d7e3277c24p-88 },
		  { -0x1.26aacbe779418p-36, 0x1.e6b067bac93acp-92 },
		  { 0x1.0c603e5994262p-38, 0x1.d6144d3f2ef17p-93 },
		  { -0x1.38f713b1343d3p-41, -0x1.0c3eef336946bp-95 } };
	  const double sc = 0x1p+24;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
      else if (fabs (fh) < 0x1.76bp-3 && sx > -10.5 && sx < -10)
	{
	  static const double x0[]
	      = { 0x1.40000093f2777p+3, 0x1.927b45d95e154p-52,
		  0x1.0780c21b6e452p-106 };
	  static const double c[][2]
	      = { { 0x1.baf825a0c63b2p+21, -0x1.20323f1015cdfp-35 },
		  { 0x1.7f3ec8ae05f2ep+18, 0x1.2aec80d23ccb8p-36 },
		  { 0x1.ba192fa62a5c8p+15, -0x1.25660ae99a81cp-39 },
		  { 0x1.1ede75ef431bp+13, -0x1.a691cbd9c9bebp-41 },
		  { 0x1.8d1b435ece20fp+10, 0x1.5b052a017c6d3p-47 },
		  { 0x1.1e4e1e218c99cp+8, 0x1.775895e089534p-46 },
		  { 0x1.a8a28e596cccep+5, -0x1.aa094ee6e63a1p-49 },
		  { 0x1.4175d0d35b3d4p+3, -0x1.0cf2eb638feb3p-51 },
		  { 0x1.ee6f0af10b985p+0, -0x1.7d3173e84c277p-54 },
		  { 0x1.80fe7b2913e68p-2, 0x1.58aa6aa5a2a1dp-56 },
		  { 0x1.2ece6307c7cbp-4, 0x1.812bbc72b9bf4p-58 },
		  { 0x1.e04bf4be0259p-7, 0x1.b2fdc34bbffbep-62 },
		  { 0x1.7f9356d1f8f5dp-9, 0x1.200577f2d649bp-63 },
		  { 0x1.3427c173faa4ep-11, 0x1.963e715185901p-65 },
		  { 0x1.f1ab0995dd7aap-14, -0x1.982b6a256e922p-70 },
		  { 0x1.93a8ac07adde7p-16, 0x1.1938f0da933adp-70 },
		  { 0x1.48b1212550a17p-18, -0x1.73810e7394decp-72 },
		  { 0x1.0c93b2c55f9b3p-20, 0x1.b11c8bd5d36cap-75 },
		  { 0x1.b8450c2eb24f2p-23, 0x1.b77d60943d51ap-83 },
		  { 0x1.69ddd961f0eb9p-25, 0x1.26555b649e2b9p-80 },
		  { 0x1.2a39767b036bdp-27, -0x1.ff547cf250c2cp-82 },
		  { 0x1.ec935873525f3p-30, -0x1.56d39c71ef428p-84 },
		  { 0x1.95e014a90402ep-32, 0x1.ff041edd80219p-88 },
		  { 0x1.4f23f078ebd25p-34, 0x1.22e3191f74383p-89 },
		  { 0x1.26ad387c7d522p-36, -0x1.0581274dbf44p-91 },
		  { 0x1.0c628e2dc8b58p-38, 0x1.4473a911adb21p-97 },
		  { 0x1.38f9f8f1d99a8p-41, -0x1.6893c733787dp-95 } };
	  const double sc = 0x1p+24;
	  double zl, zh = fasttwosum (x0[0] + sx, x0[1], &zl);
	  zl += x0[2];
	  double sh = zh * sc, sl = zl * sc;
	  int n = array_length (c), k = 6;
	  fl = sh * polyd (sh, k, c + n - k);
	  fh = polydd2 (sh, sl, n - k, c, &fl);
	  fh = muldd2 (zh, zl, fh, fl, &fl);
	}
    }

  unsigned ft = (asuint64 (fl) + 2) & (~UINT64_C(0) >> 12);
  if (ft <= 2u)
    return as_lgamma_database (sx, fh + fl);
  return fh + fl;
}

double
__ieee754_lgamma_r (double x, int *signgamp)
{
  // piece-wise polynomial approximation in [0.5, 8.29541] range
  // range borders
  static const unsigned ubrd[20]
      = { 0x1ff0000, 0x1ff146c, 0x1ff2b7b, 0x1ff4532, 0x1ff614c,
	  0x1ff8310, 0x1ff93f7, 0x1ffa880, 0x1ffc05e, 0x1ffdb73,
	  0x1fff8a5, 0x2001147, 0x2002703, 0x20041ac, 0x200622a,
	  0x20084d9, 0x2009ce7, 0x200ba2c, 0x200ddd7, 0x20104ba };
  // the region offset i.e. z = x-off for P(z)
  static const double offs[19]
      = { 0x1.146cd8p-1, 0x1.3fe898p-1, 0x1.70aea8p-1, 0x1.a67fcp-1,
	  0x1.e76db8p-1, 0x1.170838p+0, 0x1.3c78a8p+0, 0x1.68df2p+0,
	  0x1.9bd14p+0,	 0x1.d41868p+0, 0x1.0d9a64p+1, 0x1.384b8p+1,
	  0x1.68b06p+1,	 0x1.a3d6dp+1,	0x1.ebdd9p+1,  0x1.21c1p+2,
	  0x1.571368p+2, 0x1.9803e8p+2, 0x1.e74cc8p+2 };
  // polynomial coefficients low part
  static const double cl[19][8] = {
    { -0x1.18ad63ca097e9p+2, 0x1.af8e15b715c51p+2, -0x1.56213b7191ba4p+3,
      0x1.151f165a9425fp+4, -0x1.c826426e4b7cdp+4, 0x1.7c313095e4b75p+5,
      -0x1.44f3d7d848e78p+6, 0x1.13384c97ea99dp+7 },
    { -0x1.0f58e76c8d235p+1, 0x1.67c3f6b7124f6p+1, -0x1.ec78d7d8185a3p+1,
      0x1.588d6487de574p+2, -0x1.e9fbe8564220dp+2, 0x1.60dd913b80b5ep+3,
      -0x1.0465db7c895a6p+4, 0x1.7ca34d903fc3p+4 },
    { -0x1.0c505555a86b2p+0, 0x1.33d3a22d1bb51p+0, -0x1.6d2a2457f05d4p+0,
      0x1.bb1c77fad8b03p+0, -0x1.115210a553746p+1, 0x1.558e305fd694p+1,
      -0x1.b4f9a0654679fp+1, 0x1.1489e269cbf39p+2 },
    { -0x1.1170ead9585bap-1, 0x1.10c67b04495d7p-1, -0x1.19de3af9dd349p-1,
      0x1.2a34cd6e66472p-1, -0x1.40e2f93066eb8p-1, 0x1.5ddc21a559735p-1,
      -0x1.860a742d837aap-1, 0x1.ace7238771e7fp-1 },
    { -0x1.be31df8f7d605p-3, 0x1.8e33a32c94cf7p-3, -0x1.6c62efd534bf3p-3,
      0x1.53719e404a7d6p-3, -0x1.4074d1d083331p-3, 0x1.31c6c5226f2b3p-3,
      -0x1.2b062b8eedd9fp-3, 0x1.219431f82fbfcp-3 },
    { -0x1.168e45409b785p-3, 0x1.a04e5759477fp-4, -0x1.43c620bb1d77fp-4,
      0x1.027d79414ff7dp-4, -0x1.a46afc0776356p-5, 0x1.5a92c3ddb75f5p-5,
      -0x1.23d37b3b3e3b6p-5, 0x1.f66a6169fe8efp-6 },
    { -0x1.2db051283fb7ap-4, 0x1.8afce072c9222p-5, -0x1.0dcc84e49a658p-5,
      0x1.7af09a263459bp-6, -0x1.0f51524188551p-6, 0x1.8a1f8bbd73d04p-7,
      -0x1.24e389a08ab23p-7, 0x1.b5c2228d32783p-8 },
    { -0x1.3fbb4a9e75e6dp-5, 0x1.6c40332da72cp-6, -0x1.b235e2a6ed724p-7,
      0x1.0a9023dcf81a5p-7, -0x1.4e128ec28e27ap-8, 0x1.a90e4421d59e7p-9,
      -0x1.14e6becdb3889p-9, 0x1.68e6a1727f763p-10 },
    { -0x1.5365e61675f08p-6, 0x1.4fd143859dc2cp-7, -0x1.5cb1d911bcabbp-8,
      0x1.75a869d793508p-9, -0x1.9941834996ea5p-10, 0x1.c782075b40e2cp-11,
      -0x1.03ab2e70c9df2p-11, 0x1.26c9636a06719p-12 },
    { -0x1.7145b3bd2da75p-7, 0x1.3e6749a0fe63p-8, -0x1.20ea7ae3d0208p-9,
      0x1.0f17a25bb48f3p-10, -0x1.045c4de3c2101p-11, 0x1.fcc9430345441p-13,
      -0x1.fccc917990854p-14, 0x1.f41fbb5026b5p-15 },
    { 0x1.253f3fc844189p-9, -0x1.cadf5cc04da1bp-11, 0x1.73e9dbf6ed988p-12,
      -0x1.34f75abb9acfdp-13, 0x1.05502104fc072p-14, -0x1.c015daee9145bp-16,
      0x1.8af9ccda4578cp-17, -0x1.5b973bad98b6bp-18 },
    { -0x1.7f80bfa6d705ep-9, 0x1.e416c7e5d3bb3p-11, -0x1.4361a69711e0fp-12,
      0x1.c0beed7451d56p-14, -0x1.3fc0c220552b8p-15, 0x1.d09a0850c9ad6p-17,
      -0x1.5b403dca4645p-18, 0x1.07c1349c4989ap-19 },
    { -0x1.8bd8d36b6b68p-10, 0x1.ab590101636b6p-12, -0x1.e980083cba776p-14,
      0x1.23c1bb53d55c4p-15, -0x1.65be06922ac5p-17, 0x1.bfd7d06279b09p-19,
      -0x1.2127a54c7c981p-20, 0x1.79ae1f9c24de8p-22 },
    { -0x1.8db1211cc179cp-11, 0x1.6c24488bdc8e9p-13, -0x1.62882b2ca3c96p-15,
      0x1.67e36a9a5f89cp-17, -0x1.785b8294e4cf2p-19, 0x1.925c40ccc4611p-21,
      -0x1.bccb0b78110bcp-23, 0x1.f05d365676624p-25 },
    { -0x1.879379df4c28cp-12, 0x1.2e193030ccfd1p-14, -0x1.f0900c0b3c1fcp-17,
      0x1.aa304a80f3ce4p-19, -0x1.795cdc082b2dfp-21, 0x1.56025546a45a8p-23,
      -0x1.4137abecddaa9p-25, 0x1.303d852294977p-27 },
    { -0x1.7b7a8dbd38635p-13, 0x1.eab932219e072p-16, -0x1.528291d0efb42p-18,
      0x1.e86163ded7066p-21, -0x1.6be3b6446506ap-23, 0x1.15d4d64fd204ap-25,
      -0x1.b8820763832efp-28, 0x1.5ff781e6e4e19p-30 },
    { -0x1.6b1f37f261621p-14, 0x1.87d88455d6443p-17, -0x1.c3a69901a7d7cp-20,
      0x1.107e1d8456499p-22, -0x1.53f4b8e0d8be8p-25, 0x1.b3016ba9fadffp-28,
      -0x1.2175c3eb445d8p-30, 0x1.841e8df7f84e7p-33 },
    { -0x1.57cce7fdc9fe7p-15, 0x1.347c6b65ace16p-18, -0x1.27eb9df26d911p-21,
      0x1.296c0dcf0b476p-24, -0x1.354fd38659786p-27, 0x1.4a2dbe1c4af19p-30,
      -0x1.6f1651636db1p-33, 0x1.9b1457d56445ap-36 },
    { -0x1.423d487c99e54p-16, 0x1.df4d1f34022a3p-20, -0x1.7d54e9cd7e7eap-23,
      0x1.3e131c44c6382p-26, -0x1.12afb6bfa8c14p-29, 0x1.e7412bd9ebd87p-33,
      -0x1.c2ab005ebc13bp-36, 0x1.a3bbacb6ee6b7p-39 },
  };
  // polynomial coefficients high part
  static const double ch[19][13][2] = {
    { { 0x1.fdbd7c56b02b5p-2, -0x1.9f8c66985b6f3p-56 },
      { -0x1.c771ed8981f3ep+0, 0x1.8d8b72ce9b19dp-54 },
      { 0x1.1558ba7c0144dp+1, 0x1.4fc1fa0f0451cp-53 },
      { -0x1.1fa938f4d4b53p+1, -0x1.f29beb3ca3738p-53 },
      { 0x1.7f7469f6781efp+1, -0x1.b59ce1aa03545p-53 } },
    { { 0x1.71c14e711391ep-2, 0x1.2ad5eb4fb4f59p-60 },
      { -0x1.740c890bd54d3p+0, -0x1.6978dab8a116p-55 },
      { 0x1.b38de2e957c18p+0, -0x1.aba2b91749902p-55 },
      { -0x1.7ab358c51c087p+0, 0x1.46a8f1bc5883bp-55 },
      { 0x1.af1b63b322b6dp+0, 0x1.2d98d261df8f3p-55 } },
    { { 0x1.e53b12b3407e2p-3, 0x1.97cb2965d31b5p-57 },
      { -0x1.2a144e9a8b92ep+0, -0x1.bbf90d2717ba5p-54 },
      { 0x1.5adc4ef58621ep+0, 0x1.d41b3282f1d5bp-54 },
      { -0x1.fb259e2817239p-1, 0x1.a19b744867ccbp-55 },
      { 0x1.ee43256a6bfd3p-1, 0x1.880c7ca4d6687p-55 } },
    { { 0x1.0719312af823cp-3, -0x1.77ca1d8b99601p-57 },
      { -0x1.d11f75dc5be7dp-1, 0x1.997295e7f58d5p-57 },
      { 0x1.18a58180335ddp+0, -0x1.9e4f675e9e244p-58 },
      { -0x1.5aea0e9166a08p-1, 0x1.4799eb996a78bp-55 },
      { 0x1.22b448094c052p-1, -0x1.221db12561423p-56 } },
    { { 0x1.3c3b637596f8dp-1, -0x1.051b18f5744bap-56 },
      { -0x1.b9ccef0d71197p-1, -0x1.cf98e73bfb3d7p-55 },
      { 0x1.c55517304ef35p-2, 0x1.dfe2299217a1ap-57 },
      { -0x1.4230fb2a20b13p-2, -0x1.8eb1c5690348fp-57 },
      { 0x1.03aa1691c1841p-2, 0x1.a0e14e4b5a96cp-57 } },
    { { -0x1.752403c835a4dp-5, 0x1.a3a43faf6ecccp-59 },
      { -0x1.c0be76051e3a5p-2, -0x1.c737cd3ea73d9p-57 },
      { 0x1.73c36ef7bf402p-1, -0x1.40c4dff8e4c1ep-56 },
      { -0x1.458cec1d1393dp-2, -0x1.c7f148cf356efp-56 },
      { 0x1.8ec2d305516c4p-3, 0x1.9566535c9eabp-57 } },
    { { -0x1.85361b993719fp-4, -0x1.dc41ac35a716fp-58 },
      { -0x1.f3e2bae2cdf7dp-3, 0x1.6d5cae27956a4p-57 },
      { 0x1.3745220b46975p-1, 0x1.56d68f9018bb8p-60 },
      { -0x1.d29172b1a4407p-3, -0x1.93fc4238117bdp-58 },
      { 0x1.ef0f914e4a75bp-4, -0x1.6f0339a5cbb3ap-58 } },
    { { -0x1.ec2ab5aa5843ap-4, -0x1.adc658df2c1c1p-62 },
      { -0x1.a6243a7f3534cp-5, 0x1.0dc0b707b85abp-59 },
      { 0x1.04116f85f23a3p-1, 0x1.517c0b25b9233p-57 },
      { -0x1.4bb33f1abe408p-3, 0x1.cc0c1f637cea4p-58 },
      { 0x1.2ecfafae59f8fp-4, 0x1.3c57c7651ae8ap-58 } },
    { { -0x1.c8928613eb4f5p-4, 0x1.55f36a43c02bcp-62 },
      { 0x1.1151b40dad4e9p-3, 0x1.67907a753aa66p-57 },
      { 0x1.b46b0b78660acp-2, -0x1.9bcdfa3bbcd41p-56 },
      { -0x1.d9cd6009ac89dp-4, -0x1.69c4d18a5c993p-59 },
      { 0x1.73b079d35c37cp-5, -0x1.4d3891ecef09ep-59 } },
    { { -0x1.00ad2093da6e4p-4, -0x1.cbf7cf885033p-58 },
      { 0x1.391f431d39831p-2, 0x1.8fb94bb0e7df5p-56 },
      { 0x1.71d5a6e677f1cp-2, 0x1.d1dc12aaa3806p-59 },
      { -0x1.57f6fbf9108c1p-4, 0x1.4e341fb4cef78p-61 },
      { 0x1.d1e33efae7a1dp-6, 0x1.c4938a6deffbep-60 } },
    { { 0x1.d344dabcc201ep-2, 0x1.574f453e55614p-56 },
      { 0x1.3c3a02b015763p-2, -0x1.342e3d6a27dfap-56 },
      { -0x1.f5d49f62ecfd6p-5, -0x1.07444b43ab601p-60 },
      { 0x1.22abe7bbdf628p-6, 0x1.2cb184651725ap-63 },
      { -0x1.8b52066552f48p-8, 0x1.bc2dbb1b8365dp-62 } },
    { { 0x1.f22e8b160e053p-3, 0x1.89c03c62a66d7p-57 },
      { 0x1.58ae0ae32162p-1, -0x1.594df075ee813p-56 },
      { 0x1.028e87f2859fdp-2, 0x1.bf1ead4dde3d4p-58 },
      { -0x1.55b4949f3971ap-5, -0x1.2cfd594571487p-59 },
      { 0x1.4cfe08a2baa09p-7, 0x1.495ab3aeecafp-62 } },
    { { 0x1.104861734d948p-1, 0x1.32e74856dbad8p-56 },
      { 0x1.b2445e9d82006p-1, 0x1.6e48e474ddfbfp-55 },
      { 0x1.b352d20042182p-3, 0x1.a8ac4f9b7c938p-60 },
      { -0x1.e6c5b3585790ep-6, -0x1.31a8ef26cbf2ep-60 },
      { 0x1.93111b206dab4p-8, -0x1.aa3ae79b1707p-63 } },
    { { 0x1.eed49cf014c0bp-1, 0x1.bca14c01f79aep-55 },
      { 0x1.0718fe597659bp+0, 0x1.7d14012138c17p-55 },
      { 0x1.6c89e19ff8e58p-3, 0x1.12dfe29d6e296p-59 },
      { -0x1.56a9890298c3ap-6, -0x1.2181516eb15d6p-61 },
      { 0x1.deaa0ec93f6d9p-9, 0x1.e4d7a3e816168p-63 } },
    { { 0x1.990530fe5fa37p+0, -0x1.cf639a3a54f76p-56 },
      { 0x1.35e029ece68dp+0, -0x1.e3db2cbb514ebp-60 },
      { 0x1.301f23426a05fp-3, -0x1.b5ec346a456bcp-57 },
      { -0x1.de5b0dd5127b5p-7, 0x1.371374acf777fp-61 },
      { 0x1.1843ded6af0f6p-9, -0x1.7779056d714p-64 } },
    { { 0x1.3ef64cb5ced7bp+1, 0x1.c3c21b0562715p-54 },
      { 0x1.654a3f497c726p+0, 0x1.3331f28ee09bbp-54 },
      { 0x1.f9f5117f295a1p-4, -0x1.ee3d2bb334106p-58 },
      { -0x1.4bb07b47ebf8dp-7, -0x1.64c2c019b90b5p-61 },
      { 0x1.449d9854bac59p-10, -0x1.d0a2827bf227p-64 } },
    { { 0x1.de185c1178ad9p+1, 0x1.d477f1a273bfcp-55 },
      { 0x1.9539397e34b21p+0, 0x1.9743cc0cd10f2p-54 },
      { 0x1.a3e2c09f7886dp-4, -0x1.17f6c25e05338p-59 },
      { -0x1.c98eb5fc97ce2p-8, 0x1.0a5104a9f402dp-63 },
      { 0x1.74b50213890abp-11, 0x1.ff0ae56647adp-65 } },
    { { 0x1.5c2be39a4c6fdp+2, 0x1.ff2814687494cp-52 },
      { 0x1.c59e5d40889c7p+0, 0x1.299ee0827992ap-55 },
      { 0x1.5bbf97b18270ep-4, -0x1.d04ddc6346897p-60 },
      { -0x1.3a2d0322cf70ep-8, 0x1.53fe131154027p-65 },
      { 0x1.a8c6d657c0cfdp-12, -0x1.b402fb82b45efp-66 } },
    { { 0x1.f07834a362b11p+2, -0x1.738a86a953af8p-52 },
      { 0x1.f68034cafc0d3p+0, 0x1.b8d6c9e2cd7d4p-56 },
      { 0x1.1f68e6efd00fap-4, -0x1.6083738e28e87p-61 },
      { -0x1.ad889b8da1552p-9, 0x1.1325e8a48689dp-64 },
      { 0x1.e0ae44f526429p-13, -0x1.997df9412e4aap-67 } },
  };

  uint64_t t = asuint64 (x);
  uint64_t nx = t << 1;
  if (__glibc_unlikely (nx >= UINT64_C(0xfeaea9b24f16a34c)))
    {
      // |x| >= 0x1.006df1bfac84ep+1015
      *signgamp = 1;
      if (t == UINT64_C(0x7f5754d9278b51a6))
	return 0x1.ffffffffffffep+1023 - 0x1p+969;
      if (t == UINT64_C(0x7f5754d9278b51a7))
	return 0x1.fffffffffffffp+1023 - 0x1p+969;
      if (__glibc_unlikely (nx >= (UINT64_C(0x7ff) << 53)))
	{			      /* x=NaN or +/-Inf */
	  if (nx == (UINT64_C(0x7ff) << 53)) /* x=+/-Inf */
	    return fabs (x);	      /* +Inf */
	  return x
		 + x; /* x=NaN, where x+x ensures the "Invalid operation"
			 exception is set if x is sNaN, and it yields a qNaN */
	}
      /* The C standard says that if the function overflows,
	 errno is set to ERANGE. */
      if (t >> 63)
	return __math_divzero (0);
      else
	return 0x1.fp1023 * 0x1.fp1023; // huge positive integer
    }
  double fx = floor (x);
  if (__glibc_unlikely (fx == x))
    { /* x is integer */
      if (x <= 0.0)
	{
	  *signgamp = 1 - 2 * (t >> 63);
	  return __math_divzero (0);
	}
      if (x == 1.0 || x == 2.0)
	{
	  *signgamp = 1;
	  return 0.0;
	}
    }
  unsigned int au = nx >> 38;
  double fh, fl, eps;
  if (au < ubrd[0])
    { // |x|<0.5
      *signgamp = 1 - 2 * (t >> 63);
      double ll, lh = as_logd (fabs (x), &ll);
      if (au < 0x1da0000)
	{ // |x|<0x1p-75
	  fh = -lh;
	  fl = -ll;
	  eps = 1.5e-22;
	}
      else if (au < 0x1fd0000)
	{ // |x|<0.03125
	  static const double c0[][2]
	      = { { -0x1.2788cfc6fb619p-1, 0x1.6cb9a4ff7c53bp-58 },
		  { 0x1.a51a6625307d3p-1, 0x1.18722054895e9p-56 },
		  { -0x1.9a4d55beab2d7p-2, -0x1.74ded0474fe66p-63 },
		  { 0x1.151322ac7d848p-2, 0x1.825b3df1d5722p-56 } };
	  static const double q[]
	      = { -0x1.a8b9c17aa5d3dp-3, 0x1.5b40cb100b9bfp-3,
		  -0x1.2703a1e13bcbcp-3, 0x1.010b36b6afdc1p-3,
		  -0x1.c8062dd09ec62p-4, 0x1.9a018c7345316p-4,
		  -0x1.7578ea8068cc4p-4, 0x1.566b51c990008p-4 };
	  double z = x, z2 = z * z, z4 = z2 * z2;
	  double q0 = q[0] + z * q[1], q2 = q[2] + z * q[3],
		 q4 = q[4] + z * q[5], q6 = q[6] + z * q[7];
	  fl = z * ((q0 + z2 * q2) + z4 * (q4 + z2 * q6));
	  fh = polydddfst (z, 4, c0, &fl);
	  fh = mulddd2 (x, fh, fl, &fl);
	  fh = sumdd (-lh, -ll, fh, fl, &fl);
	  eps = 1.5e-22;
	}
      else
	{
	  // -0.5<x<-0.03125 || 0.03125<x<0.5 and thus x+1 in [0.5, 0.96875] +
	  // [1.03125, 1.5] and covered by the piece-wise approximation
	  double xl;
	  double tf = fasttwosum (1, asdouble (t), &xl);
	  au = asuint64 (tf) >> 37;
	  unsigned int ou = au - ubrd[0];
	  int j = ((0x157ced865ul - ou * 0x150d) * ou + 0x128000000000) >> 45;
	  j -= au < ubrd[j];
	  double z = (tf - offs[j]) + xl, z2 = z * z, z4 = z2 * z2;
	  const double *q = cl[j];
	  double q0 = q[0] + z * q[1], q2 = q[2] + z * q[3],
		 q4 = q[4] + z * q[5], q6 = q[6] + z * q[7];
	  fl = z * ((q0 + z2 * q2) + z4 * (q4 + z2 * q6));
	  fh = polydddfst (z, 5, ch[j], &fl);
	  if (__glibc_unlikely (j == 4))
	    { // treat the region around the root at 1
	      z = -x;
	      fh = mulddd2 (z, fh, fl, &fl);
	    }
	  eps = fabs (fh) * 8.3e-20;
	  fh = sumdd (-lh, -ll, fh, fl, &fl);
	  eps += fabs (lh) * 5e-22;
	}
    }
  else
    {
      double ax = fabs (x);
      if (au >= ubrd[19])
	{ // |x|>=8.29541 we use asymptotic expansion or Stirling's formula
	  double ll, lh = as_logd (ax, &ll);
	  lh -= 1;
	  // (x-0.5)*ln(x) = (x-0.5)*(lh + ll)
	  if (__glibc_unlikely (au >= 0x2198000))
	    { // x >= 0x1p52
	      // for large |x| use expansion
	      if (__glibc_unlikely (au >= 0x3fabaa6))
		lh = fasttwosum (lh, ll, &ll); // x>=0x1.754cp+1014
	      double hlh = lh * 0.5;
	      lh = mulddd2 (ax, lh, ll, &ll);
	      ll -= hlh;
	    }
	  else
	    {
	      // for other |x| use a simple product
	      lh = mulddd2 (ax - 0.5, lh, ll, &ll);
	    }
	  static const double c[][2]
	      = { { 0x1.acfe390c97d6ap-2, -0x1.1d9792ced423ap-58 },
		  { 0x1.55555555554c1p-4, -0x1.0143af34001bdp-59 } };
	  static const double q[]
	      = { -0x1.6c16c1697de08p-9, 0x1.a019f47b230fdp-11,
		  -0x1.380aab821e42ep-11, 0x1.b617d2c5b5b66p-11,
		  -0x1.a7fd66a05ccfcp-10 };
	  lh = fastsum (lh, ll, c[0][0], c[0][1], &ll);
	  if (ax < 0x1p100)
	    {
	      double zh = 1.0 / ax, zl = fma (zh, -ax, 1.0) * zh;
	      double z2h = zh * zh, z4h = z2h * z2h;
	      double q0 = q[0] + z2h * q[1], q2 = q[2] + z2h * q[3], q4 = q[4];
	      fl = z2h * (q0 + z4h * (q2 + z4h * q4));
	      fh = fasttwosum (c[1][0], fl, &fl);
	      fl += c[1][1];
	      fh = muldd2 (fh, fl, zh, zl, &fl);
	    }
	  else
	    {
	      fh = 0;
	      fl = 0;
	    }
	  fh = fastsum (lh, ll, fh, fl, &fl);
	  eps = fabs (fh) * 4.5e-20;
	}
      else
	{ // x in [0.5, 8.29541] range
	  unsigned ou = au - ubrd[0];
	  int j = ((0x157ced865ul - ou * 0x150d) * ou + 0x128000000000) >> 45;
	  j -= au < ubrd[j];
	  double z = ax - offs[j], z2 = z * z, z4 = z2 * z2;
	  const double *q = cl[j];
	  double q0 = q[0] + z * q[1], q2 = q[2] + z * q[3],
		 q4 = q[4] + z * q[5], q6 = q[6] + z * q[7];
	  fl = z * ((q0 + z2 * q2) + z4 * (q4 + z2 * q6));
	  fh = polydddfst (z, 5, ch[j], &fl);
	  if (__glibc_unlikely (j == 4))
	    { // treat the region around the root at 1
	      z = 1 - ax;
	      fh = mulddd2 (z, fh, fl, &fl);
	    }
	  if (__glibc_unlikely (j == 10))
	    { // treat the region around the root at 2
	      z = ax - 2;
	      fh = mulddd2 (z, fh, fl, &fl);
	    }
	  eps = fabs (fh) * 8.3e-20 + 1e-24;
	}
      if (t >> 63)
	{ // x<0 so use reflection formula
	  double sl, sh = as_sinpipid (x - floor (x), &sl);
	  sh = mulddd2 (-x, sh, sl, &sl);
	  double ll, lh = as_logd (sh, &ll);
	  ll += sl / sh;
	  fh = -sumdd (fh, fl, lh, ll, &fl);
	  fl = -fl;
	  eps += fabs (lh) * 4e-22;
	  int64_t k = fx;
	  *signgamp = 1 - 2 * (k & 1);
	}
      else
	{
	  *signgamp = 1;
	}
    }
  double ub = fh + (fl + eps), lb = fh + (fl - eps);
  if (ub != lb)
    { // rounding test
      return as_lgamma_accurate (x);
    }
  return ub;
}
libm_alias_finite (__ieee754_lgamma_r, __lgamma_r)

double as_logd (double x, double *l)
{
  static const struct
  {
    uint16_t c0;
    short c1;
  } B[] = {
    { 301, 27565 },    { 7189, 24786 },	  { 13383, 22167 },  { 18923, 19696 },
    { 23845, 17361 },  { 28184, 15150 },  { 31969, 13054 },  { 35231, 11064 },
    { 37996, 9173 },   { 40288, 7372 },	  { 42129, 5657 },   { 43542, 4020 },
    { 44546, 2457 },   { 45160, 962 },	  { 45399, -468 },   { 45281, -1838 },
    { 44821, -3151 },  { 44032, -4412 },  { 42929, -5622 },  { 41522, -6786 },
    { 39825, -7905 },  { 37848, -8982 },  { 35602, -10020 }, { 33097, -11020 },
    { 30341, -11985 }, { 27345, -12916 }, { 24115, -13816 }, { 20661, -14685 },
    { 16989, -15526 }, { 13107, -16339 }, { 9022, -17126 },  { 4740, -17889 }
  };
  static const double r1[] = {
    0x1p+0,	 0x1.f508p-1, 0x1.ea4ap-1, 0x1.dfcap-1, 0x1.d582p-1,
    0x1.cb72p-1, 0x1.c19ap-1, 0x1.b7f8p-1, 0x1.ae8ap-1, 0x1.a55p-1,
    0x1.9c4ap-1, 0x1.9374p-1, 0x1.8acep-1, 0x1.8258p-1, 0x1.7a12p-1,
    0x1.71f8p-1, 0x1.6a0ap-1, 0x1.6248p-1, 0x1.5abp-1,	0x1.5342p-1,
    0x1.4bfep-1, 0x1.44ep-1,  0x1.3deap-1, 0x1.371ap-1, 0x1.307p-1,
    0x1.29eap-1, 0x1.2388p-1, 0x1.1d48p-1, 0x1.172cp-1, 0x1.113p-1,
    0x1.0b56p-1, 0x1.059cp-1, 0x1p-1,
  };
  static const double r2[]
      = { 0x1p+0,      0x1.ffa7p-1, 0x1.ff4fp-1, 0x1.fef6p-1, 0x1.fe9ep-1,
	  0x1.fe45p-1, 0x1.fdedp-1, 0x1.fd94p-1, 0x1.fd3cp-1, 0x1.fce4p-1,
	  0x1.fc8cp-1, 0x1.fc34p-1, 0x1.fbdcp-1, 0x1.fb84p-1, 0x1.fb2cp-1,
	  0x1.fad4p-1, 0x1.fa7cp-1, 0x1.fa24p-1, 0x1.f9cdp-1, 0x1.f975p-1,
	  0x1.f91ep-1, 0x1.f8c6p-1, 0x1.f86fp-1, 0x1.f817p-1, 0x1.f7cp-1,
	  0x1.f769p-1, 0x1.f711p-1, 0x1.f6bap-1, 0x1.f663p-1, 0x1.f60cp-1,
	  0x1.f5b5p-1, 0x1.f55ep-1, 0x1.f507p-1 };
  static const double l1[][2]
      = { { 0x0p+0, 0x0p+0 },
	  { 0x1.9f5e440f128dbp-37, 0x1.62d07abp-6 },
	  { -0x1.527d64b444fa3p-37, 0x1.62f483dp-5 },
	  { 0x1.3aff57187d0cfp-39, 0x1.0a267214p-4 },
	  { -0x1.4634c201e2b9cp-41, 0x1.62e04bcp-4 },
	  { -0x1.d46364a8017c7p-36, 0x1.bb9db708p-4 },
	  { -0x1.882b6acb3f696p-36, 0x1.0a29f69cp-3 },
	  { 0x1.5a5833aeff542p-37, 0x1.368507dap-3 },
	  { -0x1.3876d32b0cbf5p-36, 0x1.62e4116cp-3 },
	  { 0x1.f5712171380e6p-37, 0x1.8f41d568p-3 },
	  { 0x1.fc0b2e87a92c1p-36, 0x1.bb98bc4cp-3 },
	  { 0x1.44c7ceb2f93f2p-36, 0x1.e7f71f08p-3 },
	  { 0x1.a147c39e44ebap-37, 0x1.0a2bfe2cp-2 },
	  { 0x1.36d8fc46707d1p-37, 0x1.205afe03p-2 },
	  { -0x1.0fd8155ea585p-37, 0x1.3685b589p-2 },
	  { 0x1.8954f1c1b010fp-37, 0x1.4cb42e19p-2 },
	  { -0x1.5d0bcd7fa4afap-36, 0x1.62e3e78cp-2 },
	  { -0x1.b0a96458bf187p-36, 0x1.79123647p-2 },
	  { 0x1.c543eab5348b9p-36, 0x1.8f422996p-2 },
	  { -0x1.15143e5c177e1p-37, 0x1.a5711c7ep-2 },
	  { 0x1.3be09bf52475cp-38, 0x1.bb9c3cebp-2 },
	  { -0x1.9b3b32e71e21dp-40, 0x1.d1cd255bp-2 },
	  { -0x1.8f02175f93786p-38, 0x1.e7fb0671p-2 },
	  { -0x1.c5fb374b7ddcfp-36, 0x1.fe2980ecp-2 },
	  { -0x1.8e174c5571bbdp-36, 0x1.0a2aef35p-1 },
	  { 0x1.fa33ff819b3ecp-36, 0x1.15420d49p-1 },
	  { 0x1.23d2634096ca6p-38, 0x1.2058ca79p-1 },
	  { 0x1.c8afc264146b2p-38, 0x1.2b7156ffp-1 },
	  { 0x1.e21780abaa301p-37, 0x1.3686c62p-1 },
	  { 0x1.3d67aee28cdc4p-36, 0x1.419f01cdp-1 },
	  { 0x1.ccd8a77731be8p-36, 0x1.4cb504d68p-1 },
	  { 0x1.0cc7dc4dbbcfdp-37, 0x1.57cb333b8p-1 },
	  { 0x1.1cf79abc9e3b4p-36, 0x1.62e42fef8p-1 } };
  static const double l2[][2] = { { 0x0p+0, 0x0p+0 },
				  { 0x1.2ccace5b018a7p-36, 0x1.641ef4p-11 },
				  { -0x1.88a5cd275513ap-36, 0x1.623d3fp-10 },
				  { -0x1.0006a77b80a2dp-38, 0x1.0a4531p-9 },
				  { 0x1.81a0ebe451ddp-39, 0x1.627a998p-9 },
				  { 0x1.4297627f3b4acp-37, 0x1.bbc015p-9 },
				  { 0x1.afb8521676db1p-36, 0x1.0a0a0c8p-8 },
				  { 0x1.080b4c8bf43cap-36, 0x1.36bc4ap-8 },
				  { 0x1.bbcdc4ef244f5p-37, 0x1.62f5a48p-8 },
				  { -0x1.eb4354215c794p-36, 0x1.8f36a44p-8 },
				  { -0x1.020c23741371bp-36, 0x1.bb7f4b8p-8 },
				  { -0x1.ff1289819f095p-36, 0x1.e7cf9d4p-8 },
				  { 0x1.9b1383de1a3f4p-36, 0x1.0a13cdep-7 },
				  { 0x1.7fa1788e44213p-38, 0x1.2043a52p-7 },
				  { 0x1.f22c04fdaaa38p-37, 0x1.3677558p-7 },
				  { -0x1.d9828c736de23p-36, 0x1.4caee08p-7 },
				  { -0x1.41b9d7994644p-36, 0x1.62ea474p-7 },
				  { 0x1.aced891ec8e07p-36, 0x1.79298b2p-7 },
				  { -0x1.e4c5365e893ffp-36, 0x1.8f2be4ep-7 },
				  { 0x1.e8d32fe7dc6f5p-36, 0x1.a572dbep-7 },
				  { -0x1.3bf707f8ee0b7p-36, 0x1.bb7cd5p-7 },
				  { -0x1.c60b95a619b91p-36, 0x1.d1cb84ap-7 },
				  { 0x1.64cbc2b83b45cp-37, 0x1.e7dd224p-7 },
				  { -0x1.68543f75f32c6p-37, 0x1.fe338fcp-7 },
				  { 0x1.421a5be17c2ecp-36, 0x1.0a266bbp-6 },
				  { -0x1.f6b329a1da537p-37, 0x1.1534f84p-6 },
				  { -0x1.9a217f361c264p-37, 0x1.2065ff9p-6 },
				  { 0x1.3856e49eac8ddp-40, 0x1.2b78651p-6 },
				  { 0x1.c6c3e72a945a1p-39, 0x1.368cb54p-6 },
				  { -0x1.dcfdc96d3a1c6p-36, 0x1.41a2f0dp-6 },
				  { 0x1.a0da4813daf37p-38, 0x1.4cbb185p-6 },
				  { 0x1.b4d3084ac1ad1p-36, 0x1.57d52c8p-6 } };
  uint64_t t = asuint64 (x);
  int ex = t >> 52;
  if (__glibc_unlikely (ex == 0))
    {
      int k = stdc_leading_zeros (t);
      t <<= k - 11;
      ex -= k - 12;
    }
  int e = ex - 0x3ff;
  t &= ~UINT64_C(0) >> 12;
  double ed = e;
  uint64_t i = t >> (52 - 5);
  int64_t d = t & (~UINT64_C(0) >> 17);
  uint64_t j
      = (t + ((uint64_t) B[i].c0 << 33) + ((int64_t) B[i].c1 * (d >> 16)))
	>> (52 - 10);
  t |= (int64_t) 0x3ff << 52;
  int i1 = j >> 5, i2 = j & 0x1f;
  double r = r1[i1] * r2[i2];
  double tf = asdouble (t);
  double o = r * tf, dxl = fma (r, tf, -o), dxh = o - 1;
  static const double c[] = { -0x1.fffffffffffd3p-2, 0x1.55555555543d5p-2,
			      -0x1.000002bb2d74ep-2, 0x1.999a692c56e4ep-3 };
  double dx = fma (r, tf, -1), dx2 = dx * dx;
  double f = dx2 * ((c[0] + dx * c[1]) + dx2 * (c[2] + dx * c[3]));
  double lt = (l1[i1][1] + l2[i2][1]) + ed * 0x1.62e42fef8p-1;
  double lh = lt + dxh, ll = (lt - lh) + dxh;
  ll += ((l1[i1][0] + l2[i2][0]) + 0x1.1cf79abc9e3b4p-36 * ed) + dxl;
  ll += f;
  *l = ll;
  return lh;
}

double
as_logd_accurate (double x, double *l, double *l_)
{
  static const struct
  {
    uint16_t c0;
    short c1;
  } B[] = {
    { 301, 27565 },    { 7189, 24786 },	  { 13383, 22167 },  { 18923, 19696 },
    { 23845, 17361 },  { 28184, 15150 },  { 31969, 13054 },  { 35231, 11064 },
    { 37996, 9173 },   { 40288, 7372 },	  { 42129, 5657 },   { 43542, 4020 },
    { 44546, 2457 },   { 45160, 962 },	  { 45399, -468 },   { 45281, -1838 },
    { 44821, -3151 },  { 44032, -4412 },  { 42929, -5622 },  { 41522, -6786 },
    { 39825, -7905 },  { 37848, -8982 },  { 35602, -10020 }, { 33097, -11020 },
    { 30341, -11985 }, { 27345, -12916 }, { 24115, -13816 }, { 20661, -14685 },
    { 16989, -15526 }, { 13107, -16339 }, { 9022, -17126 },  { 4740, -17889 }
  };
  static const double r1[] = {
    0x1p+0,	 0x1.f508p-1, 0x1.ea4ap-1, 0x1.dfcap-1, 0x1.d582p-1,
    0x1.cb72p-1, 0x1.c19ap-1, 0x1.b7f8p-1, 0x1.ae8ap-1, 0x1.a55p-1,
    0x1.9c4ap-1, 0x1.9374p-1, 0x1.8acep-1, 0x1.8258p-1, 0x1.7a12p-1,
    0x1.71f8p-1, 0x1.6a0ap-1, 0x1.6248p-1, 0x1.5abp-1,	0x1.5342p-1,
    0x1.4bfep-1, 0x1.44ep-1,  0x1.3deap-1, 0x1.371ap-1, 0x1.307p-1,
    0x1.29eap-1, 0x1.2388p-1, 0x1.1d48p-1, 0x1.172cp-1, 0x1.113p-1,
    0x1.0b56p-1, 0x1.059cp-1, 0x1p-1,
  };
  static const double r2[]
      = { 0x1p+0,      0x1.ffa7p-1, 0x1.ff4fp-1, 0x1.fef6p-1, 0x1.fe9ep-1,
	  0x1.fe45p-1, 0x1.fdedp-1, 0x1.fd94p-1, 0x1.fd3cp-1, 0x1.fce4p-1,
	  0x1.fc8cp-1, 0x1.fc34p-1, 0x1.fbdcp-1, 0x1.fb84p-1, 0x1.fb2cp-1,
	  0x1.fad4p-1, 0x1.fa7cp-1, 0x1.fa24p-1, 0x1.f9cdp-1, 0x1.f975p-1,
	  0x1.f91ep-1, 0x1.f8c6p-1, 0x1.f86fp-1, 0x1.f817p-1, 0x1.f7cp-1,
	  0x1.f769p-1, 0x1.f711p-1, 0x1.f6bap-1, 0x1.f663p-1, 0x1.f60cp-1,
	  0x1.f5b5p-1, 0x1.f55ep-1, 0x1.f507p-1 };
  static const double h1[][3] = {
    { 0x0p+0, 0x0p+0, 0x0p+0 },
    { 0x1.7052459b95dfcp-95, 0x1.d79103c4a36a4p-43, 0x1.62d07ab33p-6 },
    { 0x1.6eaee7553baap-99, 0x1.60a6d2eec1732p-43, 0x1.62f483cea8p-5 },
    { 0x1.a79804e7ea455p-95, 0x1.aff57187d0cecp-43, 0x1.0a26721424p-4 },
    { 0x1.3d01ad8f4bdb1p-95, 0x1.ce59eff0ea32p-44, 0x1.62e04bbff4p-4 },
    { 0x1.28aff4e83414bp-97, 0x1.ce4dabff41cbp-43, 0x1.bb9db70628p-4 },
    { 0x1.947079fb56ea3p-96, 0x1.ea4a9a604b4f2p-43, 0x1.0a29f69b3ap-3 },
    { 0x1.071b8f347abdcp-95, 0x1.2c19d77faa108p-44, 0x1.368507da56p-3 },
    { 0x1.60dc9eae5e8c7p-95, 0x1.c4966a79a05a8p-43, 0x1.62e4116b62p-3 },
    { 0x1.19de1930ac389p-95, 0x1.5c485c4e03992p-43, 0x1.8f41d5687cp-3 },
    { 0x1.38f75cf02d618p-95, 0x1.65d0f5258168p-49, 0x1.bb98bc4cfep-3 },
    { 0x1.66d0ece8cc055p-99, 0x1.8f9d65f27e3cp-45, 0x1.e7f71f08a2p-3 },
    { 0x1.110c0a2d0d02ap-97, 0x1.47c39e44eb9ap-45, 0x1.0a2bfe2c34p-2 },
    { 0x1.c2e1c7624937cp-95, 0x1.b63f119c1f42ap-43, 0x1.205afe0326p-2 },
    { 0x1.80a68afc2eecp-98, 0x1.3f550ad3d7d8p-48, 0x1.3685b588dep-2 },
    { 0x1.2cb8203dc0bb6p-95, 0x1.54f1c1b010eb8p-45, 0x1.4cb42e1931p-2 },
    { 0x1.e234ee6f272d2p-95, 0x1.7a19402da833cp-43, 0x1.62e3e78ba8p-2 },
    { 0x1.3d19c1b2c67cbp-96, 0x1.ab4dd3a073c68p-43, 0x1.7912364693p-2 },
    { 0x1.62e1c0308ab55p-95, 0x1.43eab5348b948p-44, 0x1.8f42299671p-2 },
    { 0x1.9b5e4e30c5f0fp-95, 0x1.75e0d1f440f44p-44, 0x1.a5711c7dddp-2 },
    { 0x1.32ba52f147867p-99, 0x1.7c137ea48eb8p-43, 0x1.bb9c3ceb13p-2 },
    { 0x1.32f3b1a0f392dp-95, 0x1.262668c70ef16p-43, 0x1.d1cd255af9p-2 },
    { 0x1.078661f35a007p-96, 0x1.fbd140d90f48p-47, 0x1.e7fb0670e7p-2 },
    { 0x1.0a4b6082eb02fp-96, 0x1.02645a4111858p-43, 0x1.fe2980eb8ep-2 },
    { 0x1.8f3673c008d9dp-95, 0x1.e8b3aa8e4433cp-44, 0x1.0a2aef34cep-1 },
    { 0x1.e4109a84a717ep-96, 0x1.19ffc0cd9f62p-43, 0x1.15420d493fp-1 },
    { 0x1.b400798743957p-96, 0x1.e931a04b652cp-45, 0x1.2058ca7909p-1 },
    { 0x1.3046edc6794ap-96, 0x1.15f84c828d642p-43, 0x1.2b7156ff0ep-1 },
    { 0x1.5cb5072b4b776p-95, 0x1.0bc055d518094p-44, 0x1.3686c6201ep-1 },
    { 0x1.051804915de87p-97, 0x1.67aee28cdc41cp-44, 0x1.419f01cd278p-1 },
    { 0x1.748ebb6888301p-98, 0x1.b14eee637d048p-45, 0x1.4cb504d6b98p-1 },
    { 0x1.55098cdd851edp-97, 0x1.31f7136ef3f3p-43, 0x1.57cb333b908p-1 },
    { 0x1.f97b57a079a19p-103, 0x1.ef35793c7673p-45, 0x1.62e42fefa38p-1 },
  };
  static const double h2[][3] = {
    { 0x0p+0, 0x0p+0, 0x0p+0 },
    { 0x1.4826401258afcp-95, 0x1.959cb60314ep-45, 0x1.641ef496p-11 },
    { 0x1.9c71d4cdcb54bp-95, 0x1.ad196c55762e2p-43, 0x1.623d3e9dp-10 },
    { 0x1.012c2a9a87c83p-96, 0x1.ff2b108feba5p-43, 0x1.0a4530f78p-9 },
    { 0x1.437bb5bf9fc34p-95, 0x1.a0ebe451ddp-47, 0x1.627a9986p-9 },
    { 0x1.1c54e00c02c66p-96, 0x1.4bb13f9da5614p-44, 0x1.bbc01514p-9 },
    { 0x1.b959e749a5ea5p-95, 0x1.dc290b3b6d866p-43, 0x1.0a0a0c9acp-8 },
    { 0x1.9d74597e3c084p-95, 0x1.69917e8794bp-49, 0x1.36bc4a108p-8 },
    { 0x1.ee2ae1f36e35ap-98, 0x1.e6e2779227a9cp-44, 0x1.62f5a48dcp-8 },
    { 0x1.6236c8dcf0cfdp-95, 0x1.7957bd470d7fp-45, 0x1.8f36a4214p-8 },
    { 0x1.a291a656020e5p-95, 0x1.f3dc8bec8e52p-44, 0x1.bb7f4b6fcp-8 },
    { 0x1.a34920afb6399p-95, 0x1.daecfcc1ed628p-45, 0x1.e7cf9d2p-8 },
    { 0x1.7b470c6ab05c6p-95, 0x1.89c1ef0d1fa32p-43, 0x1.0a13cdeccp-7 },
    { 0x1.2c071f819920fp-95, 0x1.f42f11c88426ep-43, 0x1.2043a522ep-7 },
    { 0x1.3d45aed9f22ap-95, 0x1.16027ed551c38p-44, 0x1.36775587cp-7 },
    { 0x1.c4ec608241822p-95, 0x1.3eb9c6490ee9ep-43, 0x1.4caee0712p-7 },
    { 0x1.5c705f1ca65efp-95, 0x1.2314335cde02p-43, 0x1.62ea4735ep-7 },
    { 0x1.c6e86fe6c904cp-96, 0x1.db123d91c0e6p-45, 0x1.79298b2d6p-7 },
    { 0x1.cf83c85a09c0dp-95, 0x1.9d64d0bb600a6p-43, 0x1.8f2be4d0cp-7 },
    { 0x1.8a899399986f8p-96, 0x1.a65fcfb8de998p-45, 0x1.a572dbef4p-7 },
    { 0x1.75dd2d25f587cp-101, 0x1.1f00e23e91p-49, 0x1.bb7cd4f62p-7 },
    { 0x1.66dc5826a7f9dp-95, 0x1.f46a59e646ed8p-44, 0x1.d1cb8491cp-7 },
    { 0x1.0bdd376e8742ap-95, 0x1.32f0ae0ed1704p-43, 0x1.e7dd22458p-7 },
    { 0x1.3bcc642260db7p-95, 0x1.eaf0228334e7ep-43, 0x1.fe338fba4p-7 },
    { 0x1.fe76520a42aecp-95, 0x1.0d2df0be17626p-43, 0x1.0a266bb5p-6 },
    { 0x1.3f0eff67b7e5p-97, 0x1.4cd65e25ac96p-45, 0x1.1534f83c1p-6 },
    { 0x1.824751a1e6c5bp-99, 0x1.77a03278f66f6p-43, 0x1.2065ff8ccp-6 },
    { 0x1.506cfc4639accp-95, 0x1.c2b724f5646e6p-43, 0x1.2b7865104p-6 },
    { 0x1.a7ac1fdfa37ep-99, 0x1.b0f9caa516828p-45, 0x1.368cb540ep-6 },
    { 0x1.b61c029607481p-96, 0x1.811b4962f1cf2p-43, 0x1.41a2f0c88p-6 },
    { 0x1.2879f319383adp-97, 0x1.b49027b5e6dep-47, 0x1.4cbb1851ap-6 },
    { 0x1.f27f4e668c317p-97, 0x1.a61095835a298p-45, 0x1.57d52c86dp-6 },
    { 0x1.c301f232c0e74p-96, 0x1.b6fc11defa4a8p-43, 0x1.62f12e132p-6 },
  };

  uint64_t t = asuint64 (x);
  int ex = t >> 52;
  if (__glibc_unlikely (ex == 0))
    {
      int k = stdc_leading_zeros (t);
      t <<= k - 11;
      ex -= k - 12;
    }
  int e = ex - 0x3ff;
  t &= ~UINT64_C(0) >> 12;
  double ed = e;
  uint64_t i = t >> (52 - 5);
  int64_t d = t & (~UINT64_C(0) >> 17);
  uint64_t j
      = (t + ((uint64_t) B[i].c0 << 33) + ((int64_t) B[i].c1 * (d >> 16)))
	>> (52 - 10);
  t |= (int64_t) 0x3ff << 52;
  int i1 = j >> 5, i2 = j & 0x1f;
  double r = r1[i1] * r2[i2];
  double tf = asdouble (t);
  double o = r * tf, dxl = fma (r, tf, -o), dxh = o - 1;
  static const double c[][2]
      = { { 0x1p+0, 0x1.a193d7f59d80ap-118 },
	  { -0x1p-1, 0x1.8c7d7a8733406p-99 },
	  { 0x1.5555555555555p-2, 0x1.55555554f571dp-56 },
	  { -0x1p-2, -0x1.e516b5d7b8c15p-73 },
	  { 0x1.999999999999ap-3, -0x1.97f2898534175p-57 },
	  { -0x1.55555555554b5p-3, -0x1.3834d62d64ec4p-59 },
	  { 0x1.249249248dbdcp-3, -0x1.8aa032979ebedp-58 },
	  { -0x1.000004e71581bp-3, 0x1.2e7f17d1c0e63p-57 },
	  { 0x1.c71e5ec7051f6p-4, 0x1.217ec3dcb2f03p-58 } };
  dxh = fasttwosum (dxh, dxl, &dxl);
  double fl = dxh * (c[6][0] + dxh * (c[7][0] + dxh * (c[8][0]))),
	 fh = polydd2 (dxh, dxl, 6, c, &fl);
  fh = muldd2 (dxh, dxl, fh, fl, &fl);
  double s2 = h1[i1][2] + h2[i2][2], s1 = h1[i1][1] + h2[i2][1],
	 s0 = h1[i1][0] + h2[i2][0];
  double L0 = 0x1.62e42fefa38p-1 * ed, L1 = 0x1.ef35793c76p-45 * ed,
	 L2 = 0x1.cc01f97b57a08p-87 * ed;
  L0 += s2;
  L1 = sumdd (L1, L2, s1, s0, &L2);
  L1 = sumdd (L1, L2, fh, fl, &L2);

  L0 = fasttwosum (L0, L1, &L1);
  L1 = fasttwosum (L1, L2, &L2);

  *l = L1;
  *l_ = L2;

  return L0;
}

static const double stpi[][2]
    = { { 0x0p+0, 0x0p+0 },
	{ 0x1.c14eff99a3ff1p-64, 0x1.fff2d746c8895p-8 },
	{ -0x1.8c4d4c1bbe38bp-62, 0x1.ffcb5e52d1f36p-7 },
	{ -0x1.08ef2408930ebp-61, 0x1.7fa7329846febp-6 },
	{ -0x1.14daa07929354p-60, 0x1.ff2d8cc5320c7p-6 },
	{ 0x1.d845cf264d016p-60, 0x1.3f3289bb44643p-5 },
	{ -0x1.43aa63f69aceap-60, 0x1.7e9d144d37f33p-5 },
	{ -0x1.bc90382ed68a4p-59, 0x1.bdcc9ea69fc93p-5 },
	{ 0x1.0fbc215a3c756p-60, 0x1.fcb76a6ecccabp-5 },
	{ 0x1.72b75e84ab5e2p-58, 0x1.1da9e1f36c497p-4 },
	{ -0x1.20d100fccf991p-59, 0x1.3ccc01b453709p-4 },
	{ -0x1.f7aac846eccfdp-63, 0x1.5bbd477204bep-4 },
	{ -0x1.17799578a6651p-59, 0x1.7a78edace5e27p-4 },
	{ 0x1.0c85deb5bb812p-58, 0x1.98fa372a35c37p-4 },
	{ -0x1.67d2eb81bbf36p-60, 0x1.b73c6faf2275cp-4 },
	{ -0x1.14b2141507a9dp-63, 0x1.d53aecba7bfp-4 },
	{ -0x1.8939cffeb036cp-58, 0x1.f2f10e3ce6d42p-4 },
	{ -0x1.2f3fbb178d1c5p-57, 0x1.082d1fa7b9738p-3 },
	{ 0x1.08479c62d3d77p-57, 0x1.16b8fb743c879p-3 },
	{ -0x1.894149dc3b5f7p-57, 0x1.2519dc47527b3p-3 },
	{ -0x1.44dad213ab344p-60, 0x1.334d8a850758dp-3 },
	{ -0x1.2d415416bae28p-58, 0x1.4151d589a490fp-3 },
	{ -0x1.2e0d0b51ed237p-57, 0x1.4f24940025067p-3 },
	{ 0x1.e8045a3cf3213p-57, 0x1.5cc3a43788a3p-3 },
	{ 0x1.be4e50e1bf91fp-57, 0x1.6a2cec76fa4bp-3 },
	{ 0x1.b1e18c1f7f635p-62, 0x1.775e5b50bb365p-3 },
	{ -0x1.10946c1f6f484p-63, 0x1.8455e7f3c6e5ap-3 },
	{ 0x1.291a88889a4e6p-59, 0x1.9111927c231cfp-3 },
	{ -0x1.bedd6f9a25da4p-57, 0x1.9d8f6441cf80bp-3 },
	{ -0x1.ce108006670c7p-57, 0x1.a9cd702648a97p-3 },
	{ 0x1.4c65624119572p-61, 0x1.b5c9d2e092baap-3 },
	{ -0x1.a26e2a2682111p-57, 0x1.c182b347bfc21p-3 },
	{ 0x1.fce159c2bb59bp-59, 0x1.ccf6429be6621p-3 },
	{ -0x1.59b1cffa69603p-58, 0x1.d822bccd7d86ep-3 },
	{ 0x1.677083288397ap-57, 0x1.e30668c31224ep-3 },
	{ 0x1.9a49696faa0ecp-57, 0x1.ed9f989d4c415p-3 },
	{ 0x1.ca323e77a3345p-58, 0x1.f7eca9f938c6fp-3 },
	{ -0x1.c702625d3863bp-57, 0x1.00f6031866f76p-2 },
	{ 0x1.180cf0e52237dp-56, 0x1.05ce114cd024ap-2 },
	{ -0x1.4be56fec860b9p-56, 0x1.0a7dc060df5eep-2 },
	{ 0x1.b5d970e5d9d07p-58, 0x1.0f045755560d9p-2 },
	{ 0x1.4c32e06c67499p-58, 0x1.1361238136929p-2 },
	{ -0x1.b512d49aedaa1p-56, 0x1.179378ad51274p-2 },
	{ -0x1.161478130996dp-58, 0x1.1b9ab12ed2518p-2 },
	{ -0x1.25feb091e921fp-59, 0x1.1f762e00ced83p-2 },
	{ 0x1.3750bc95dae67p-56, 0x1.232556dcc945fp-2 },
	{ -0x1.257966a1044c5p-56, 0x1.26a79a522d332p-2 },
	{ -0x1.ac6af78c05e44p-57, 0x1.29fc6ddcbcb72p-2 },
	{ 0x1.71dbd64ba4f95p-56, 0x1.2d234df9ec8c9p-2 },
	{ 0x1.020107d2c17bp-57, 0x1.301bbe3d2b9c7p-2 },
	{ -0x1.9d4016f0b15c4p-56, 0x1.32e5496312cfcp-2 },
	{ 0x1.f557b51b587ccp-56, 0x1.357f81637a329p-2 },
	{ 0x1.c88cee9bad9f9p-57, 0x1.37e9ff82709ecp-2 },
	{ 0x1.ebde6bb284e87p-56, 0x1.3a246460134f7p-2 },
	{ -0x1.8b1d8c40ffea3p-56, 0x1.3c2e580742edap-2 },
	{ 0x1.de48797b477f2p-56, 0x1.3e0789fb33cf7p-2 },
	{ -0x1.8bc6105a80fa5p-56, 0x1.3fafb143d754bp-2 },
	{ -0x1.9f4cc680744f3p-56, 0x1.41268c791c743p-2 },
	{ 0x1.2ed295e9d0ef2p-60, 0x1.426be1cd05c06p-2 },
	{ -0x1.4a98b72ed3789p-60, 0x1.437f7f1493531p-2 },
	{ -0x1.5c080cdd72ddfp-56, 0x1.446139cf7f413p-2 },
	{ -0x1.b00c622ae015ep-57, 0x1.4510ef2ecb654p-2 },
	{ 0x1.8dd5ec4960646p-56, 0x1.458e841a1f7dap-2 },
	{ 0x1.e1f89d1adcbc6p-56, 0x1.45d9e533f6cacp-2 },
	{ -0x1.6b01ec5417056p-56, 0x1.45f306dc9c883p-2 } };

double
as_sinpipid (double x, double *l)
{
  x -= 0.5;
  double ax = fabs (x);
  double sx = ax * 128;
  double ix = roundeven_finite (sx);
  int ky = ix, kx = 64 - ky;
  if (__glibc_unlikely (kx < 2))
    {
      static const double c[2]
	  = { -0x1.a51a6625307d3p+0, -0x1.16cc8f2044a4ap-55 };
      static const double cl[] = { 0x1.9f9cb402bc42ap-1, -0x1.86a8e46ddf78dp-3,
				   0x1.ac644e7aa33e6p-6 };
      double z = 0.5 - ax, z2 = z * z, z2l = fma (z, z, -z2);
      double fl = z2 * (cl[0] + z2 * (cl[1] + z2 * (cl[2]))),
	     fh = fasttwosum (c[0], fl, &fl), e;
      fl += c[1];
      fh = muldd2 (z2, z2l, fh, fl, &fl);
      fh = mulddd2 (z, fh, fl, &fl);
      fh = fasttwosum (z, fh, &e);
      fl += e;
      *l = fl;
      return fh;
    }
  double d = ix - sx, d2 = d * d;

  double sh = stpi[kx][1], sl = stpi[kx][0];
  double ch = stpi[ky][1], cl = stpi[ky][0];
  // sin(a + d) = sin(a)*(1-d^2*P(d^2)) + cos(a)*d*Q(d^2)
  // sin(a_i + d) = s[i] + d*(c[i]*Q(d^2) - s[i]*d*P(d^2))
  // sin(a_i + d)/pi = s[i]/pi + d*(c[i]/pi*Q(d^2) - s[i]/pi*d*P(d^2))

  static const double c[] = { -0x1.3bd3cc9be45dep-12, 0x1.03c1f081b5ac4p-26,
			      -0x1.55d3c7e3bd8bfp-42, 0x1.e1f4826790653p-59 };
  double c0 = -0x1.692b66e3cf6e8p-66;
  static const double s[] = { 0x1.921fb54442d18p-6, -0x1.4abbce625be53p-19,
			      0x1.466bc67748efcp-34, -0x1.32d26e446373ap-50 };
  double s0 = 0x1.1a624b88c9448p-60;

  double P = d2 * (c[1] + d2 * (c[2] + d2 * c[3]));
  double Q = d2 * (s[1] + d2 * (s[2] + d2 * s[3]));

  double ql, qh = fasttwosum (s[0], Q, &ql);
  ql += s0;
  ch = muldd2 (qh, ql, ch, cl, &cl);
  double tl, th = fasttwosum (c[0], P, &tl);
  tl += c0;
  th = mulddd2 (d, th, tl, &tl);
  double pl, ph = muldd2 (th, tl, sh, sl, &pl);
  ch = fastsum (ch, cl, ph, pl, &cl);
  ch = mulddd2 (d, ch, cl, &cl);
  sh = fastsum (sh, sl, ch, cl, l);
  return sh;
}

double
as_sinpipid_accurate (double x, double *l)
{
  x -= 0.5;
  x = fabs (x);
  x *= 128;
  double ix = roundeven_finite (x), d = ix - x;
  int ky = ix, kx = 64 - ky;

  double sh = stpi[kx][1], sl = stpi[kx][0];
  double ch = stpi[ky][1], cl = stpi[ky][0];
  // sin(a + d) = sin(a)*(1-d^2*P(d^2)) + cos(a)*d*Q(d^2)
  // sin(a_i + d) = s[i] + d*(c[i]*Q(d^2) - s[i]*d*P(d^2))

  static const double c[][2]
      = { { -0x1.3bd3cc9be45dep-12, -0x1.692b71366c792p-66 },
	  { 0x1.03c1f081b5ac4p-26, -0x1.32b342c5da62ep-80 },
	  { -0x1.55d3c7e3cbffap-42, 0x1.17fcb68af9aaep-98 },
	  { 0x1.e1f506890e556p-59, -0x1.56c9658a7cfd5p-114 },
	  { -0x1.a6d193ca649ap-76, 0x1.61669b56f0275p-134 } };
  static const double s[][2]
      = { { 0x1.921fb54442d18p-6, 0x1.1a62633145c07p-60 },
	  { -0x1.4abbce625be53p-19, 0x1.05511c684796p-73 },
	  { 0x1.466bc6775aae2p-34, -0x1.6dc0d14c26b21p-89 },
	  { -0x1.32d2cce62bd86p-50, 0x1.2ba6dbc4b37cp-104 },
	  { 0x1.50783487e6b5cp-67, -0x1.b77efed0f9c1fp-122 },
	  { -0x1.e306ec8cf7c02p-85, 0x1.5d2601f85289ap-139 } };

  double d2h = d * d, d2l = fma (d, d, -d2h);
  double Pl = 0, Ph = polydd2 (d2h, d2l, 5, c, &Pl);
  double Ql = 0, Qh = polydd2 (d2h, d2l, 6, s, &Ql);

  Ph = mulddd2 (d, Ph, Pl, &Pl);
  Ph = muldd2 (sh, sl, Ph, Pl, &Pl);
  Qh = muldd2 (ch, cl, Qh, Ql, &Ql);

  ch = fastsum (Qh, Ql, Ph, Pl, &cl);
  ch = mulddd2 (d, ch, cl, &cl);
  sh = fastsum (sh, sl, ch, cl, l);
  return sh;
}

double
as_lgamma_asym_accurate (double xh, double *xl, double *e)
{
  // x*ln(x) - x - 0.5*ln(x) + 0.5*ln(2*pi)
  // (x-0.5)*ln(x) - (x - 0.5) - 0.5 + 0.5*ln(2*pi)
  // (x-0.5)*(ln(x)-1) + 0.5*(ln(2*pi)-1)

  double l2, l1, l0 = as_logd_accurate (xh, &l1, &l2), l0x, l1x, l2x;
  if (xh < 0x1p120)
    {
      double zh = 1.0 / xh, dz = *xl * zh,
	     zl = (fma (zh, -xh, 1.0) - dz) * zh;
      if (*xl != 0)
	{
	  double dl2, dl1 = mulddd2 (*xl, zh, fma (zh, -xh, 1.0) * zh, &dl2);
	  dl2 -= dl1 * dl1 / 2;
	  l1 = sumdd (l1, l2, dl1, dl2, &l2);
	}

      double wl, wh;
      if (asuint64 (xh) >> 52 > 0x3ff + 51)
	{
	  wh = xh;
	  wl = *xl - 0.5;
	}
      else
	{
	  wh = xh - 0.5;
	  wl = *xl;
	}
      l0 -= 1;

      l0x = l0 * wh;
      double l0xl = fma (l0, wh, -l0x);
      l1x = l1 * wh;
      double l1xl = fma (l1, wh, -l1x);
      l2x = l2 * wh;

      l1x = sumdd (l1x, l2x, l0xl, l1xl, &l2x);
      l1x = sumdd (l1x, l2x, l0 * wl, l1 * wl, &l2x);
      double z2l, z2h = muldd2 (zh, zl, zh, zl, &z2l);
      double fh, fl;
      if (xh >= 48)
	{
	  static const double c[][2]
	      = { { 0x1.acfe390c97d69p-2, 0x1.3494bc9001766p-56 },
		  { 0x1.5555555555555p-4, 0x1.55555554133c7p-58 },
		  { -0x1.6c16c16c16c17p-9, 0x1.f4c03199d8517p-64 },
		  { 0x1.a01a01a01a016p-11, 0x1.fb315e77b4883p-66 },
		  { -0x1.381381380c1bp-11, -0x1.c4cc418316ed1p-65 },
		  { 0x1.b951e22dd8dfcp-11, 0x1.e8da392824ecfp-65 },
		  { -0x1.f6a875bb1ab7bp-10, 0x1.27c5fcbab6b5dp-64 },
		  { 0x1.a0a6926f4992p-8, -0x1.1f355cbf82229p-63 } };
	  l1x = sumdd (l1x, l2x, c[0][0], c[0][1], &l2x);
	  fl = 0;
	  fh = polydd2 (z2h, z2l, 7, c + 1, &fl);
	}
      else if (xh >= 14.5)
	{
	  static const double c[][2]
	      = { { 0x1.acfe390c97d69p-2, 0x1.3494bc9007f28p-56 },
		  { 0x1.5555555555555p-4, 0x1.5555554ad7655p-58 },
		  { -0x1.6c16c16c16c17p-9, 0x1.f4b73ea546bd4p-64 },
		  { 0x1.a01a01a01a01ap-11, -0x1.464e31b1a609ap-65 },
		  { -0x1.3813813813692p-11, 0x1.63676fd3c851ep-66 },
		  { 0x1.b951e2b143b5ep-11, 0x1.cfe48021143d1p-65 },
		  { -0x1.f6ab0d459cadbp-10, 0x1.770df5ee0beeap-64 },
		  { 0x1.a41a211f4e098p-8, -0x1.e00b3c1619519p-63 },
		  { -0x1.e41f97ad4634dp-6, 0x1.05ffdbc72560fp-60 },
		  { 0x1.6f15ef2b47719p-3, -0x1.5665cba69dbaap-57 },
		  { -0x1.5762c9f49fe25p+0, 0x1.afeff5294ad13p-54 },
		  { 0x1.2ea102098f818p+3, 0x1.609db97f1bc89p-51 } };
	  l1x = sumdd (l1x, l2x, c[0][0], c[0][1], &l2x);
	  fl = 0;
	  fh = polydd2 (z2h, z2l, 11, c + 1, &fl);
	}
      else
	{
	  static const double c[][2]
	      = { { 0x1.acfe390c97d69p-2, 0x1.3494bce9b5c5p-56 },
		  { 0x1.5555555555555p-4, 0x1.55551a0d18a1dp-58 },
		  { -0x1.6c16c16c16c17p-9, 0x1.07171d4a61bb9p-63 },
		  { 0x1.a01a01a01a008p-11, -0x1.49af39145d6ep-65 },
		  { -0x1.38138138124ccp-11, -0x1.b8f71068f5292p-66 },
		  { 0x1.b951e2b09b07p-11, -0x1.3c8a4c099ae72p-66 },
		  { -0x1.f6ab0d4de4a5cp-10, -0x1.fbb4ed542d17ep-65 },
		  { 0x1.a41a384fafd09p-8, 0x1.1510cc5dec148p-64 },
		  { -0x1.e4277d1a2b2e4p-6, 0x1.33cdd66b223e7p-61 },
		  { 0x1.6fdf731005805p-3, -0x1.a5a306692214p-57 },
		  { -0x1.641de6a9b2f1cp+0, 0x1.27cfac68728a3p-56 },
		  { 0x1.aa463d5553a9fp+3, 0x1.aeba9a5b525f3p-52 },
		  { -0x1.312d3aa56b6a2p+7, -0x1.2b2c4e643e5b9p-48 },
		  { 0x1.f3e8a4cd3d268p+10, -0x1.ec8b086b0215ep-45 },
		  { -0x1.ba9362f228307p+14, 0x1.b8ecae7dba56fp-40 },
		  { 0x1.8ed4df00421e4p+18, 0x1.c620cd11f44a5p-36 },
		  { -0x1.5af889993596dp+22, -0x1.23a78dbbfe515p-32 },
		  { 0x1.1789aff6ddc93p+26, 0x1.fe7e161d8bdb1p-29 },
		  { -0x1.94353def0e5f8p+29, -0x1.64a798491c211p-25 },
		  { 0x1.ffb9253861b9cp+32, -0x1.469d6acfea485p-23 },
		  { -0x1.159c1244c9ee4p+36, -0x1.f7491d4abcc7bp-19 },
		  { 0x1.f991ba9776c6ap+38, 0x1.cf2c83cfe2e55p-19 },
		  { -0x1.795f45ae3fa72p+41, -0x1.a777148d59de4p-13 },
		  { 0x1.c05845a910b48p+43, 0x1.433e08525d4bdp-11 },
		  { -0x1.96c399c145d71p+45, 0x1.d495c9370dc8ap-11 },
		  { 0x1.083bb9c79529bp+47, -0x1.1640e2de8cf9cp-8 },
		  { -0x1.b53232e707f4bp+47, 0x1.6d239acd71b5cp-8 },
		  { 0x1.59bad61bd81d5p+47, 0x1.0e3f2ea42a0ep-7 } };
	  l1x = sumdd (l1x, l2x, c[0][0], c[0][1], &l2x);
	  fl = 0;
	  fh = polydd2 (z2h, z2l, array_length (c) - 1, c + 1, &fl);
	}
      fh = muldd2 (zh, zl, fh, fl, &fl);
      l1x = sumdd (l1x, l2x, fh, fl, &l2x);
      l0x = fasttwosum (l0x, l1x, &l1x);
      l1x = fasttwosum (l1x, l2x, &l2x);
    }
  else
    {
      double wl = *xl - 0.5;
      l0 -= 1;
      l0x = l0 * xh;
      double l0xl = fma (l0, xh, -l0x);
      l1x = l1 * xh;
      double l1xl = fma (l1, xh, -l1x);
      l2x = l2 * xh;
      l1x = sumdd (l1x, l2x, l0xl, l1xl, &l2x);
      l1x = sumdd (l1x, l2x, l0 * wl, l1 * wl, &l2x);
    }
  *xl = l1x;
  *e = l2x;
  return l0x;
}
