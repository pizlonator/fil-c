/* Correctly-rounded true gamma function for binary64 value.

Copyright (c) 2024-2025 Alexei Sibidanov <sibid@uvic.ca>.

The original version of this file was copied from the CORE-MATH
project (file src/binary64/tgamma/tgamma.c, revision 0f185e23).

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
#include <math.h>
#include <float.h>
#include <libm-alias-finite.h>
#include "math_config.h"
#include "ddcoremath.h"

static double __attribute__ ((noinline)) as_logd (double, double *);
static double __attribute__ ((noinline)) as_expd (double, double *, int *);
static double __attribute__ ((noinline)) as_sinpid (double, double *);
static double __attribute__ ((noinline)) as_lgamma_asym (double, double *);

/* split x into a high part xh (return value) and a low part xl,
   where ulp(xh) = 2^-25. If x <= 2^26, the splitting is exact:
   x = xh + xl */
static inline double
splt (double x, double *xl)
{
  static const double off = 0x1.8p27;
  double xh = (x + off) - off;
  *xl = x - xh;
  return xh;
}

static inline double
sprod (double x, double x0, double x1, double x2, double *l1, double *l2)
{
  double z0 = x * x0, z0l = fma (x, x0, -z0), z1 = x * x1,
	 z2 = x * x2 + fma (x, x1, -z1), e;
  z0 = splt (z0, &e);
  e = fasttwosum (e, z0l, &z0l);
  *l1 = twosum (e, z1, &e);
  *l2 = e + z0l + z2;
  return z0;
}

static double
poly3 (double d, unsigned imax, const double ch[], unsigned jmax,
       const double cl[][2], double *ol)
{
  double t0 = 1, t1 = 0, t2 = 0;
  double s0 = ch[0], s1 = 0, s2 = 0;
  for (unsigned i = 1; i < imax; i++)
    {
      t0 = sprod (d, t0, t1, t2, &t1, &t2);
      s0 += t0 * ch[i];
      double fl, fh = mulddd2 (ch[i], t1, t2, &fl);
      s1 = sumdd (s1, s2, fh, fl, &s2);
    }
  double fl = 0, fh = polyddd (d, jmax, cl, &fl);
  s1 = sumdd (s1, s2, fh, fl, &s2);
  s0 = fasttwosum (s0, s1, &s1);
  s1 = fasttwosum (s1, s2, &s2);
  *ol = s1;
  return s0;
}

static __attribute__ ((noinline)) double
as_tgamma_database (double x, double f)
{
  static const double db[][3] = {
    { -0x1.48ba8e27d09adp+7, -0x1.0b34f909c5c92p-976, 0x0.01p-1022 },
    { -0x1.1fe464bbe8b7ap+7, 0x1.f6e94380a86bfp-826, 0x1p-880 },
    { -0x1.dfe438a574b34p+6, 0x1.e0efc1ffa409ep-656, 0x1p-710 },
    { -0x1.c008ed7e2be92p+6, -0x1.2820dd1286d5ep-599, -0x1p-653 },
    { -0x1.bcc2b6af0ebaep+6, 0x1.5742e6ca2fe43p-598, -0x1p-652 },
    { -0x1.2c0358d14dacep+6, 0x1.c8e82e0e0a4f6p-356, 0x1p-410 },
    { -0x1.f126edde91b5bp+5, -0x1.f4cdd1a52b2e3p-283, -0x1p-337 },
    { -0x1.d97de88bda2dfp+5, 0x1.2227eb4f08b21p-265, 0x1p-319 },
    { -0x1.ce749a6427fddp+5, 0x1.30d786820381fp-257, -0x1p-311 },
    { -0x1.ccb10c3d47943p+5, 0x1.b9e53a96c9939p-257, -0x1p-311 },
    { -0x1.c9cc11aba9632p+5, 0x1.644a102fa86bp-254, 0x1p-308 },
    { -0x1.b93b0669b1556p+5, 0x1.03a95aab1bc81p-241, 0x1p-295 },
    { -0x1.67893c596ef0cp+5, -0x1.1bd662d0bc936p-182, 0x1p-236 },
    { -0x1.ae39c32c36e42p+4, -0x1.82b46d1babd86p-90, -0x1p-144 },
    { -0x1.8d5826734a06p+4, -0x1.ae95b301e8bf3p-81, 0x1p-135 },
    { -0x1.849b47bda8526p+4, -0x1.8c2973252464cp-79, 0x1p-133 },
    { -0x1.0ba2acf2de6b2p+4, -0x1.ca9739fd7435ep-46, -0x1p-100 },
    { -0x1.f49bb1a25a54ep+3, 0x1.fbf8e7755e967p-42, 0x1p-96 },
    { -0x1.9057ede749837p+3, -0x1.eb68bf9d04126p-30, 0x1p-84 },
    { -0x1.65509f6aed026p+3, 0x1.c3791f14051e1p-24, 0x1p-78 },
    { -0x1.5f353a2d26238p+3, -0x1.20e6a9093e033p-20, 0x1p-74 },
    { -0x1.3be29766cccacp+3, 0x1.8fad89ade334bp-19, 0x1p-73 },
    { -0x1.e33cfdfb73bcdp+2, 0x1.abe0430abe7dep-13, -0x1p-67 },
    { -0x1.e11a9d07e14aap+2, 0x1.c58ee82102e5p-13, 0x1p-67 },
    { -0x1.9945148859d8p+2, -0x1.1d4187d2d1e32p-9, 0x1p-63 },
    { -0x1.3fc07c80057fdp+2, -0x1.14fd6b28fb843p+1, -0x1p-53 },
    { -0x1.ca5042fd026bep+1, 0x1.fe72b07f8530ap-3, 0x1p-57 },
    { -0x1.4f977a4a186e9p+1, -0x1.c7082ecde156ap-1, -0x1p-55 },
    { -0x1.2db99b79dfe45p+1, -0x1.394da6c1b5e01p+0, 0x1p-54 },
    { -0x1.d71c18bba5e34p+0, 0x1.e1e710f476c6bp+1, 0x1p-53 },
    { -0x1.9b4cf5c6b37edp+0, 0x1.285a86b08aca2p+1, 0x1p-53 },
    { -0x1.4c20c91b866ffp+0, 0x1.ad49175ae3fa8p+1, 0x1p-53 },
    { -0x1.3dea2db193059p+0, 0x1.02d76ec3da035p+2, -0x1p-52 },
    { -0x1.2147794f4b43bp+0, 0x1.dcaa532ea5c3bp+2, -0x1p-52 },
    { -0x1.f4180137777fp-1, -0x1.5bab35ff72e22p+5, -0x1p-49 },
    { -0x1.89c1327cd62e6p-1, -0x1.481a01786e77p+2, 0x1p-52 },
    { -0x1.74a9509402866p-1, -0x1.2378573c2914ep+2, 0x1p-52 },
    { -0x1.0aa724e38e0c9p-1, -0x1.c648650a45953p+1, 0x1p-53 },
    { -0x1.ee9a5ac162dc1p-2, -0x1.c69d8a8fa985ep+1, 0x1p-53 },
    { -0x1.de31773767db1p-2, -0x1.c883c55f5d3a2p+1, -0x1p-53 },
    { -0x1.0e17b51a5c1cbp-2, -0x1.2de378f77ff2dp+2, 0x1p-52 },
    { 0x1.c05aa42cb27fep-2, 0x1.02f80f15c9486p+1, 0x1p-53 },
    { 0x1.38828fbbe134p-1, 0x1.774353be6bfa6p+0, -0x1p-54 },
    { 0x1.5f35406cd126p-1, 0x1.522440edb9679p+0, -0x1p-54 },
    { 0x1.09ef8f46ee74bp+0, 0x1.f5443da4bc3bep-1, -0x1p-55 },
    { 0x1.0a9070c11f0c7p+0, 0x1.f4a2abf00601ap-1, -0x1p-55 },
    { 0x1.94fcb07ab7f61p+0, 0x1.c88182e08d193p-1, -0x1p-55 },
    { 0x1.bca8ea6404514p+0, 0x1.d512dc4822b38p-1, -0x1p-55 },
    { 0x1.e0c9a45452d7cp+0, 0x1.e8acd192e461ep-1, 0x1p-55 },
    { 0x1.616cd9ea484abp+1, 0x1.9f85cce39e731p+0, 0x1p-54 },
    { 0x1.a1d899263d9a1p+2, 0x1.2f2fb8e4d1274p+8, 0x1p-46 },
    { 0x1.b163c719149afp+2, 0x1.d76e1ede08821p+8, -0x1p-46 },
    { 0x1.43714b74fe055p+6, 0x1.ecc3784ce1ffp+393, 0x1p+339 },
    { 0x1.676921a72fecfp+6, 0x1.76e0e21ee3989p+451, 0x1p+397 },
    { 0x1.f3505ba057812p+6, 0x1.068712884fb42p+687, 0x1p+633 },
    { 0x1.303ed951d434p+7, 0x1.fb70d4503e49bp+880, -0x1p+826 },
    { 0x1.3a0b358e9e93bp+7, 0x1.81a5fa517374fp+916, 0x1p+862 },
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
as_tgamma_accurate (double x)
{
  if (fabs (x) < 0.25)
    {
      static const double cc[][2]
	  = { { -0x1.2788cfc6fb619p-1, 0x1.6cb90701fbfbap-58 },
	      { 0x1.fa658c23b1578p-1, 0x1.dd92b465a81ddp-55 },
	      { -0x1.d0a118f324b63p-1, 0x1.3a4f48373c073p-58 },
	      { 0x1.f6a51055096b5p-1, 0x1.fabe4f73da654p-56 },
	      { -0x1.f6c80ec38b67bp-1, 0x1.c9fc797e7567ep-55 },
	      { 0x1.fc7e0a6eb310bp-1, -0x1.042340fb21e2fp-58 },
	      { -0x1.fdf3f157b7a39p-1, -0x1.6fd12a61ae3a9p-55 },
	      { 0x1.ff07b5a17ff6cp-1, -0x1.9b8f7ad70e4bcp-55 },
	      { -0x1.ff803d68a0bd4p-1, 0x1.ed4f2b2dcc156p-59 },
	      { 0x1.ffc0841d585a3p-1, -0x1.7089ffbe760fp-55 },
	      { -0x1.ffe018c484f48p-1, 0x1.f8a632e2ff912p-55 },
	      { 0x1.fff00b768f1c4p-1, 0x1.50de70bb4e28bp-56 },
	      { -0x1.fff8035584df4p-1, -0x1.aae8f6d8b868dp-55 },
	      { 0x1.fffc012f95019p-1, 0x1.d26498825213dp-55 },
	      { -0x1.fffe0062afaf7p-1, 0x1.ef9359641ed4bp-55 },
	      { 0x1.ffff002146fecp-1, 0x1.1ce6548eaa3cp-55 },
	      { -0x1.ffff800af4ca8p-1, 0x1.67121c2223cb7p-56 },
	      { 0x1.ffffc0037a50dp-1, -0x1.7c2694e8ff6afp-57 },
	      { -0x1.ffffe00676eccp-1, -0x1.2563151dfa334p-55 },
	      { 0x1.fffff00ac53bfp-1, 0x1.3effd975c6fecp-55 },
	      { -0x1.fffff72b5b5d1p-1, 0x1.fb25bd33b12e8p-55 },
	      { 0x1.fffffa828c9f3p-1, 0x1.5fde8ff2fb275p-55 },
	      { -0x1.00000bc9c338dp+0, -0x1.7ced37913c915p-56 },
	      { 0x1.000014750b54cp+0, 0x1.980564dfb043cp-54 },
	      { -0x1.fffdabf3f1dfbp-1, -0x1.e34acb3149e71p-55 },
	      { 0x1.fffc7cf7439c3p-1, -0x1.2599c887c744ap-55 },
	      { -0x1.001452e7ff0b7p+0, -0x1.acc4a235f0de4p-56 },
	      { 0x1.001c6c5b18192p+0, 0x1.35874aa96ce14p-54 } };
      static const double c[]
	  = { -0x1.fdf5126e6a83bp-1, 0x1.fd56c1b531709p-1,
	      -0x1.0956741759e58p+0, 0x1.0b6167d27c5c4p+0,
	      -0x1.8e69e55b6dcap-1,  0x1.7e124660c827dp-1,
	      -0x1.c797b42bd6745p+0, 0x1.d688bb8db046cp+0 };
      double x2 = x * x, x4 = x2 * x2;
      double c0 = c[0] + x * c[1] + x2 * (c[2] + x * c[3]);
      double c4 = c[4] + x * c[5] + x2 * (c[6] + x * c[7]);
      c0 += x4 * c4;

      double cl = x * c0, ch = polyddd (x, 28, cc, &cl);
      double fh = 1.0 / x, dh = fma (fh, -x, 1.0), fl = dh * fh,
	     fll = fma (fl, -x, dh) * fh;
      fl = sumdd (fl, fll, ch, cl, &fll);
      fl = twosum (fl, fll, &fll);
      fh = fasttwosum (fh, fl, &fl);
      fl = fasttwosum (fl, fll, &fll);
      double et;
      fasttwosum (fh, 2 * fl, &et);
      if (et == 0)
	{
	  if (copysign (1, fl) * copysign (1, fll) > 0)
	    fl *= 1 + 0x1p-50;
	  else
	    fl *= 1 - 0x1p-50;
	}
      return fh + fl;
    }

  double ix = floor (x), d = 2 * (x - (ix + 0.5));
  int i = ix, jm, eoff;
  double fh, fl;
  if (i > 159)
    {
      static const double ch[]
	  = { 0x1.0079p+0, 0x1.4569p+1, 0x1.9cfbp+1, 0x1.5d7e8p+1, 0x1.bbc2p+0,
	      0x1.c2dep-1, 0x1.7dd4p-2, 0x1.154p-3,  0x1.606p-5,   0x1.8ep-7,
	      0x1.96p-9,   0x1.78p-11,	0x1.4p-13,   0x1p-15 };
      static const double cl[][2]
	  = { { -0x1.35efdada260fp-23, -0x1.8f77d4b23a815p-77 },
	      { 0x1.7df9be2dd4bfep-18, -0x1.57dcebb38b14ap-72 },
	      { -0x1.f2ea2f115251dp-18, 0x1.5db7a1637f6f9p-72 },
	      { -0x1.ba2ee68beb692p-18, -0x1.379a1eea94b56p-73 },
	      { -0x1.453a3cbf93b46p-19, -0x1.caedf955a404fp-74 },
	      { -0x1.0705e77cba5p-18, 0x1.7c27c7f017abbp-72 },
	      { 0x1.85047ccb18f1ep-19, 0x1.48c618ef405c2p-73 },
	      { -0x1.7af537fc58007p-18, -0x1.b513687e96fb8p-75 },
	      { -0x1.6b0d2669b1e53p-19, 0x1.818f0e841525p-73 },
	      { 0x1.3ffea7715e965p-18, 0x1.6db8e88404f0ap-72 },
	      { -0x1.f63410ae5ac04p-18, 0x1.e58afac170173p-74 },
	      { -0x1.5d6da3bdb1b14p-19, -0x1.3b475b11ba7a4p-76 },
	      { -0x1.22c3a134bb9e5p-20, -0x1.fac524517c258p-77 },
	      { -0x1.cd01e4fcf1p-21, -0x1.93b7c28c25e1p-76 },
	      { 0x1.69e4ebf3e520ap-18, 0x1.c8120c75b1cd4p-73 },
	      { 0x1.eb6d0cfb27c49p-21, -0x1.1ecce3a7c493p-75 },
	      { 0x1.38e164032e836p-23, -0x1.962783da9a188p-77 },
	      { 0x1.770fa90c60f7ap-26, 0x1.5d7127e890bc4p-80 },
	      { 0x1.a8b8bd80a645dp-29, -0x1.74eeaf30435a1p-83 },
	      { 0x1.c7c03bc5e8d14p-32, 0x1.2c25a03df992ap-86 },
	      { 0x1.d0b4225377931p-35, 0x1.ddb1297fb3cc9p-92 },
	      { 0x1.c35fd7323a155p-38, 0x1.d4774c8e26f9p-92 },
	      { 0x1.a298bcbc155fap-41, -0x1.650026f575beep-95 },
	      { 0x1.73684debaa9f6p-44, 0x1.58d03f212eaecp-98 },
	      { 0x1.3be174b363d46p-47, 0x1.8a28d3d4b2148p-103 },
	      { 0x1.01f81326f4e6bp-50, -0x1.91908a55064f3p-106 },
	      { 0x1.953cbad515517p-54, -0x1.420ffc1db04c7p-109 },
	      { 0x1.32925a5ea9c65p-57, -0x1.9e94d39e65107p-111 },
	      { 0x1.bf668201467f3p-61, 0x1.56890ec45063cp-115 },
	      { 0x1.3b3c32c75d257p-64, 0x1.dfaa5f59e084ep-118 },
	      { 0x1.ad258392d239dp-68, 0x1.cb0d9ef739c01p-122 },
	      { 0x1.1c4de9410ecadp-71, 0x1.6792403d59171p-125 },
	      { 0x1.7ae04b02dd534p-75, 0x1.93a2496f85817p-129 },
	      { 0x1.acbc408b92b88p-79, 0x1.7ad0987f2e913p-133 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 160 - i;
      eoff = 942;
    }
  else if (i > 127)
    {
      static const double ch[]
	  = { 0x1.9496p-1, 0x1.eac5p+0, 0x1.29c08p+1, 0x1.e1e3p+0, 0x1.248fp+0,
	      0x1.1c46p-1, 0x1.cc88p-3, 0x1.3fep-4,   0x1.84cp-6,  0x1.a4p-8,
	      0x1.98p-10,  0x1.7p-12,	0x1.4p-14,    0x1p-16 };
      static const double cl[][2]
	  = { { 0x1.8e707e9176d4ep-18, 0x1.18eb4b2587c6bp-72 },
	      { 0x1.c647adb963f23p-21, -0x1.b26e8986d3017p-75 },
	      { 0x1.6886696cb9659p-18, -0x1.697ce1c89986fp-72 },
	      { 0x1.16285b78f812ap-18, -0x1.8ee808b24a62bp-76 },
	      { -0x1.68335ce0107eap-18, -0x1.7e002876bc274p-72 },
	      { 0x1.9cb055b4f8942p-19, 0x1.c94d03e5eb637p-73 },
	      { -0x1.4c6bf6842f51dp-21, -0x1.3e43b6edbaacap-77 },
	      { -0x1.c7106a068341fp-18, 0x1.195285bab5ec2p-72 },
	      { 0x1.f5998fe3d915fp-18, -0x1.5948c0e9510ebp-73 },
	      { 0x1.97b0180d0c5f5p-18, -0x1.11410994a517ap-73 },
	      { 0x1.2a21a8d31c72bp-18, -0x1.19ad99344e42dp-73 },
	      { -0x1.76ac336149616p-18, -0x1.e86bde11a9a81p-72 },
	      { -0x1.a15f563f0b167p-18, 0x1.36b11fc19411cp-73 },
	      { -0x1.1deb5989b9a3p-19, -0x1.bfdc7707da2eap-73 },
	      { 0x1.32a52525f4863p-19, 0x1.f85b217b7d774p-76 },
	      { 0x1.8e93c045c1cf8p-22, 0x1.9cad2ac270a72p-77 },
	      { 0x1.e5d8b386f2d64p-25, -0x1.820c77564cd71p-81 },
	      { 0x1.16c8506f985d5p-27, 0x1.e7e180d6fac2ap-81 },
	      { 0x1.2e41d5e5f0a4bp-30, 0x1.360871c61556ep-84 },
	      { 0x1.368f32a797fbap-33, -0x1.87cc8925213b6p-89 },
	      { 0x1.2f3b3045158f1p-36, -0x1.b42574ac644aap-90 },
	      { 0x1.1a10f6a02627ap-39, -0x1.5e6cc913c895bp-99 },
	      { 0x1.f5100472f2101p-43, 0x1.e485e076a616bp-98 },
	      { 0x1.a9d3f94b0e784p-46, 0x1.0e407c106b58ep-100 },
	      { 0x1.5aeb873725533p-49, 0x1.482c0d148c26bp-103 },
	      { 0x1.0f69e30b483c5p-52, 0x1.674f383d8090cp-106 },
	      { 0x1.9879f300bf8a1p-56, -0x1.19ca77e2fe1e5p-113 },
	      { 0x1.2816bf4968093p-59, -0x1.c16acd78f35e5p-113 },
	      { 0x1.9dffd9c4fa0d7p-63, -0x1.b4280170f91a4p-118 },
	      { 0x1.1751481bc6761p-66, -0x1.d0b5cdc9e8b59p-121 },
	      { 0x1.6e40af3edb3b8p-70, 0x1.3da803a35bf4cp-126 },
	      { 0x1.e1e8285879a4dp-74, -0x1.e0ba5a096ee05p-129 },
	      { 0x1.0ec77abc3346ap-77, -0x1.ac5f2c2aa6632p-131 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 128 - i;
      eoff = 713;
    }
  else if (i > 95)
    {
      static const double ch[]
	  = { 0x1.f9eep-1, 0x1.20a8p+1, 0x1.498cp+1, 0x1.f5e4p+0, 0x1.1ec7p+0,
	      0x1.065p-1,  0x1.9018p-3, 0x1.05ap-4,  0x1.2b8p-6,  0x1.31p-8,
	      0x1.18p-10,  0x1.ep-13,	0x1.8p-15 };
      static const double cl[][2]
	  = { { 0x1.c78cd57b8f02ep-19, -0x1.8c6c3dfa6aafep-74 },
	      { -0x1.141e76cd562bbp-21, -0x1.7a6effaf310ffp-76 },
	      { -0x1.b3bfffd9645f9p-23, 0x1.d0be736d9ea75p-78 },
	      { -0x1.74a101cbcea97p-19, -0x1.bd3aa654f0d59p-73 },
	      { 0x1.1f7115750e069p-18, -0x1.95d25b304262ap-74 },
	      { 0x1.b91c47e155385p-23, -0x1.8e4d746fd0075p-77 },
	      { -0x1.bb86f8f5c65c2p-19, 0x1.381bdbd4c1c5bp-73 },
	      { 0x1.cd0650618cb0ap-18, 0x1.9688706a3db21p-72 },
	      { 0x1.aa7f4480b06b5p-18, -0x1.6db50a59f90bbp-72 },
	      { 0x1.7268062231b9bp-20, -0x1.3fb68176b596fp-74 },
	      { -0x1.04f22e54f0afbp-20, 0x1.e02ebcb9e86e2p-75 },
	      { -0x1.acd49a07fb2b5p-18, -0x1.309feee83299bp-73 },
	      { -0x1.b201681361e3cp-19, -0x1.159c15b7d4d21p-78 },
	      { 0x1.f8279d8aa1d7bp-18, -0x1.98f1053b36c68p-73 },
	      { 0x1.4ad47fe92e846p-20, 0x1.9c7afd69fcdc2p-78 },
	      { 0x1.956f032ef399ep-23, -0x1.a69375bda764dp-77 },
	      { 0x1.d2072b342925ap-26, -0x1.f392ab37bca41p-83 },
	      { 0x1.f868142b56aa9p-29, -0x1.b6cee9e438fbfp-83 },
	      { 0x1.01ede1370ca43p-31, 0x1.ffd62a5b184f4p-85 },
	      { 0x1.f409c99b792a5p-35, -0x1.f2131330468a5p-89 },
	      { 0x1.ccaefeaee2271p-38, 0x1.c8f2629b8d1f6p-93 },
	      { 0x1.94676dbf2542p-41, -0x1.93363fe2f6303p-95 },
	      { 0x1.5305612768bb2p-44, 0x1.1a5310aba4ab9p-100 },
	      { 0x1.0ffa5b925ce6fp-47, -0x1.37b6a905b0ef1p-101 },
	      { 0x1.a2656e8fad62dp-51, -0x1.3a327c3656cffp-105 },
	      { 0x1.3516bc8a6740dp-54, 0x1.414cf70cf8d99p-109 },
	      { 0x1.b7514d20d05f2p-58, -0x1.716a9bf7fc4dfp-112 },
	      { 0x1.2cc1700d2e504p-61, -0x1.48bce98e6490bp-116 },
	      { 0x1.8cfd88b5e7cf7p-65, -0x1.4a7ab42096b4fp-119 },
	      { 0x1.fc50fc5a359f9p-69, 0x1.813d73fa7c688p-123 },
	      { 0x1.4595e9eb5cd68p-72, 0x1.0cbcc598f3247p-129 },
	      { 0x1.67509ddcf771ep-76, 0x1.2afe131ae173dp-131 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 96 - i;
      eoff = 495;
    }
  else if (i > 63)
    {
      static const double ch[]
	  = { 0x1.fd48p-1, 0x1.08c2p+1, 0x1.1386p+1, 0x1.7ea5p+0, 0x1.8eeap-1,
	      0x1.4dp-2,   0x1.cfbp-4,	0x1.15p-5,   0x1.22p-7,	  0x1.0ep-9,
	      0x1.cp-12,   0x1.4p-14,	0x1p-16 };
      static const double cl[][2]
	  = { { 0x1.5707d010a8f9ep-18, 0x1.1cd385e4fc0f9p-76 },
	      { -0x1.58b73a5631c6p-18, 0x1.1d3603c734ffdp-73 },
	      { -0x1.3140a3ad6ae57p-18, -0x1.da8e7c85d715ep-73 },
	      { 0x1.6b0c5338e5384p-20, 0x1.efc2c921fa207p-74 },
	      { 0x1.824ecd8850473p-18, -0x1.509b36bf6318fp-72 },
	      { -0x1.39fae2bb0a7b8p-22, -0x1.5c1a0cae8774ep-77 },
	      { 0x1.76aadfd438322p-19, 0x1.045572bdbb76cp-73 },
	      { -0x1.f99e20df21f82p-19, -0x1.e66544233f3bdp-73 },
	      { -0x1.e66db8450df87p-18, -0x1.899ca09ee0617p-72 },
	      { -0x1.36a70e91aaa64p-19, -0x1.95f52cb796fc7p-73 },
	      { 0x1.0e5fa77b36437p-18, 0x1.dc1d5188b6ee8p-72 },
	      { 0x1.8f82eddfdf12dp-18, 0x1.f96b0393cd92ep-78 },
	      { -0x1.d2517c38f4223p-21, 0x1.ef82090bb6937p-76 },
	      { 0x1.383219e9f6c6dp-19, -0x1.d5f4e489d4b4cp-73 },
	      { 0x1.772f83f07674ap-22, -0x1.1078add968846p-76 },
	      { 0x1.a52d1ce181a4cp-25, -0x1.5cfd72cddd6f6p-79 },
	      { 0x1.bb9f07f338f7ap-28, 0x1.f815d2263f4ap-82 },
	      { 0x1.b8236b99402e9p-31, 0x1.6ad738311aa1cp-85 },
	      { 0x1.9cc29e9133becp-34, -0x1.fb04c2e039096p-88 },
	      { 0x1.6f02b67c4c01ap-37, 0x1.96ee3d672ff9cp-92 },
	      { 0x1.364416a73a182p-40, -0x1.c471b8d12f423p-94 },
	      { 0x1.f402cfa1d2176p-44, -0x1.09eb27385e9c1p-98 },
	      { 0x1.80e4505fae2a2p-47, -0x1.89b3ab7c8cfc8p-101 },
	      { 0x1.1b9ee5ece1b68p-50, -0x1.34f81a87fae7ep-105 },
	      { 0x1.90e43d92e9cb4p-54, 0x1.54bb8ddddd531p-108 },
	      { 0x1.1034e7e674987p-57, -0x1.027786f083934p-111 },
	      { 0x1.63b88b2da3df3p-61, 0x1.50e6876f58749p-116 },
	      { 0x1.bff6e7ec38ee2p-65, 0x1.d26042ee70922p-119 },
	      { 0x1.100e634de8248p-68, -0x1.742620952ba9ap-122 },
	      { 0x1.4046bb4643d14p-72, 0x1.f7bc9779b9709p-136 },
	      { 0x1.77deb8521aacfp-76, 0x1.8523ae2540fa8p-130 },
	      { 0x1.82bcd04f34df6p-80, 0x1.9c7c0c516c99fp-134 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 64 - i;
      eoff = 293;
    }
  else if (i > 31)
    {
      static const double ch[]
	  = { 0x1.1d8ep+0, 0x1.eed6p+0, 0x1.adddp+0, 0x1.f32cp-1,
	      0x1.b3d4p-2, 0x1.313p-3,	0x1.65p-5,   0x1.67p-7,
	      0x1.3cp-9,   0x1.fp-12,	0x1.8p-14,   0x1p-16 };
      static const double cl[][2]
	  = { { 0x1.08788e5afe5dep-18, 0x1.3ee3cd2a75802p-74 },
	      { 0x1.858860fe7bccbp-21, 0x1.0ceddfcb3586cp-77 },
	      { -0x1.e32f925f74881p-23, -0x1.c8be268af7c68p-77 },
	      { -0x1.9a65ca00dc604p-18, 0x1.3664b3fa1f36ep-72 },
	      { 0x1.c5c11cd8da405p-20, -0x1.e2437a70d9247p-74 },
	      { -0x1.7d49b382dc165p-18, 0x1.69f02bb1656e9p-72 },
	      { 0x1.14365c1b9a8dcp-19, -0x1.64190c0310f4ep-74 },
	      { -0x1.264ca62f5b521p-18, -0x1.d054f7489b23dp-72 },
	      { 0x1.72e83cdc675dcp-19, 0x1.89c0b0a3674dcp-73 },
	      { 0x1.f355500214418p-21, 0x1.303e6a2bb42aap-77 },
	      { -0x1.fe131bca3fe2ep-18, 0x1.36c088eef6972p-79 },
	      { -0x1.cae961f5880f4p-20, -0x1.9b584ebcee7p-74 },
	      { 0x1.0da3a3b8dd7b6p-19, 0x1.de0818c1103d1p-73 },
	      { 0x1.27e24b54f5efcp-22, 0x1.c16bcb66079d6p-76 },
	      { 0x1.2e24b46ea5b9cp-25, 0x1.6ae734aab6297p-80 },
	      { 0x1.20948894ade9ep-28, -0x1.ce21f3b3e69d3p-82 },
	      { 0x1.02f1573b1894fp-31, -0x1.2885bf7979555p-85 },
	      { 0x1.b6453cc9db81cp-35, 0x1.b885dc6a6e672p-90 },
	      { 0x1.5f01b66699e4fp-38, -0x1.86d1947a5527dp-92 },
	      { 0x1.0adc466fcfcc4p-41, 0x1.10184dbc0aa14p-95 },
	      { 0x1.8240d0955089fp-45, -0x1.3e1359fb3cb01p-99 },
	      { 0x1.0abe8782bcb76p-48, 0x1.898bdd66d4264p-105 },
	      { 0x1.605c88eb0b0e2p-52, 0x1.d367940f1eaf7p-107 },
	      { 0x1.be13c09b8cb35p-56, 0x1.b0d6934f0969dp-110 },
	      { 0x1.0f1bb7e179b3bp-59, -0x1.4758d0f583b01p-113 },
	      { 0x1.3cf0d7e4564b9p-63, -0x1.ab7dacbe5a77cp-118 },
	      { 0x1.64cd2b355bd94p-67, 0x1.3300f36dbdb1bp-122 },
	      { 0x1.8440addcd6f88p-71, -0x1.2983c9b95f53p-130 },
	      { 0x1.a288c864c407ep-75, 0x1.eaeadc5c2a302p-133 },
	      { 0x1.936e038d60b53p-79, -0x1.3364d18a07032p-133 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 32 - i;
      eoff = 115;
    }
  else if (i > 25)
    {
      static const double ch[]
	  = { 0x1.047p+0,  0x1.a8468p+0, 0x1.5ad7p+0, 0x1.7b64p-1,
	      0x1.3852p-2, 0x1.9cc8p-4,	 0x1.c82p-6,  0x1.b18p-8,
	      0x1.6ap-10,  0x1.1p-12,	 0x1.8p-15,   0x1p-17 };
      static const double cl[][2]
	  = { { 0x1.a7b3763fc86d4p-19, 0x1.f2bfe298eb76dp-73 },
	      { -0x1.9edf082edd847p-19, -0x1.637eef57fb3bcp-75 },
	      { 0x1.1e50c072b2042p-19, -0x1.b999e1b7bd929p-74 },
	      { -0x1.6e1a698fec4bdp-21, 0x1.3e7cf3aa75219p-75 },
	      { 0x1.ec7eb3251126ep-19, -0x1.fccfa136951d6p-73 },
	      { -0x1.75133f02463ccp-20, 0x1.55b27a0ab7fc2p-77 },
	      { -0x1.1dde62b18b04cp-22, 0x1.0b84d6dcb215ap-76 },
	      { -0x1.4ce23b6fb3257p-20, 0x1.0b3ec4cc119dfp-78 },
	      { -0x1.f24694600ef28p-20, 0x1.a624cef9a725ep-76 },
	      { -0x1.90a6a37c3c06dp-19, -0x1.2adc4f450bc1cp-73 },
	      { -0x1.6f4bf9bfd39dbp-19, -0x1.22773d6af9d2fp-73 },
	      { -0x1.17bbbc341037ap-20, -0x1.5d251c8d425d8p-81 },
	      { 0x1.f1a0eb7f8d73p-21, -0x1.3bcfc3d5e17c3p-75 },
	      { 0x1.034a47489d445p-23, -0x1.ab4fc357d6378p-77 },
	      { 0x1.f73d5e00b50b9p-27, -0x1.4bc0e702e7c2p-82 },
	      { 0x1.c911b4728cbd3p-30, 0x1.7b2b1db2787a5p-86 },
	      { 0x1.8641cdcbb6b73p-33, -0x1.7e342f0e64af3p-87 },
	      { 0x1.3a74f6feade5bp-36, 0x1.c4dbbf00254dap-91 },
	      { 0x1.dfe03d3b95b87p-40, -0x1.779af9dd23ca5p-94 },
	      { 0x1.5bca6f9345035p-43, -0x1.9809863c46a4p-97 },
	      { 0x1.e025d291bb213p-47, 0x1.86bfdbab2a35bp-103 },
	      { 0x1.3c73ef8cc9fep-50, -0x1.8487fb03e56bdp-106 },
	      { 0x1.8f298b5a16d77p-54, 0x1.2d1e9f80d0656p-108 },
	      { 0x1.e2c7aa7139c29p-58, -0x1.6194ec5ebbc6cp-112 },
	      { 0x1.18760ee1fd378p-61, 0x1.b04b22c0b3p-115 },
	      { 0x1.3976787052c12p-65, -0x1.94e76e07a7345p-119 },
	      { 0x1.523d7c6d6d11ep-69, -0x1.f07495c35a672p-123 },
	      { 0x1.68e0a8901c32ep-73, -0x1.9b92b26f525f1p-127 },
	      { 0x1.59a8bf4071da2p-77, -0x1.ded899b17529fp-131 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 26 - i;
      eoff = 86;
    }
  else if (i > 15)
    {
      static const double ch[]
	  = { 0x1.2e19p+0, 0x1.a2d2p+0, 0x1.24aep+0, 0x1.12d2p-1,
	      0x1.85fp-3,  0x1.bdcp-5,	0x1.ab8p-7,  0x1.62p-9,
	      0x1p-11,	   0x1.4p-14,	0x1p-16 };
      static const double cl[][2]
	  = { { 0x1.d0981002b8dcdp-25, -0x1.0ae8574151114p-79 },
	      { 0x1.21706a4d09ab8p-19, -0x1.32b74543eb2bep-74 },
	      { 0x1.3e8095f4cb656p-18, 0x1.bc796acd28761p-72 },
	      { 0x1.30787fd8e4581p-18, -0x1.bd47cf783c039p-74 },
	      { 0x1.88d7d3732161p-18, -0x1.20b483cf2643cp-72 },
	      { 0x1.29e7fefe96787p-18, -0x1.298d41fe9bdccp-72 },
	      { 0x1.397a3e61f6195p-19, 0x1.4349a3f7a0168p-73 },
	      { -0x1.61f8b334d0636p-20, 0x1.d10573aa993acp-76 },
	      { 0x1.d0cf2ec845cf3p-19, 0x1.fb20051e9ebe6p-75 },
	      { 0x1.000599df4de2bp-18, 0x1.4d947d89b02cbp-73 },
	      { -0x1.ce4c3c64cf0d2p-19, 0x1.cc1a5d2dcc154p-73 },
	      { 0x1.ab9d3e2c45b3ep-20, 0x1.81ef343d36f1fp-74 },
	      { 0x1.a92575df4eb3bp-23, -0x1.fb6784e2f7401p-78 },
	      { 0x1.8842770eab9c5p-26, 0x1.7b4dc0b29de35p-80 },
	      { 0x1.51ce1cfb60d71p-29, -0x1.452ce054d3324p-84 },
	      { 0x1.10e17b81bf75ep-32, -0x1.69bfe10efa7f7p-86 },
	      { 0x1.9f556d989db8bp-36, 0x1.6f1687badbf46p-90 },
	      { 0x1.2ae585ff0ac32p-39, 0x1.9d8eaa07598e9p-93 },
	      { 0x1.982eddb5fb2cap-43, -0x1.cde9b5613dc57p-97 },
	      { 0x1.093c09caa4734p-46, 0x1.fe61a1d516c3ep-102 },
	      { 0x1.48e5fa5f33d0ep-50, -0x1.9aca6f48cc057p-106 },
	      { 0x1.861601359014fp-54, 0x1.bdbb28174b55bp-108 },
	      { 0x1.bb79265fb1f49p-58, 0x1.9bf33a9bfde0ep-114 },
	      { 0x1.e4376fea4f434p-62, -0x1.a0590b281c112p-120 },
	      { 0x1.fcb050fe822abp-66, 0x1.06b1825b5f657p-124 },
	      { 0x1.0173552d965efp-69, -0x1.dfebaea36ba1fp-129 },
	      { 0x1.f79f2fdba95fap-74, 0x1.2387fbe549d2p-128 },
	      { 0x1.e6141ae71b61dp-78, -0x1.e48646743a55ap-132 },
	      { 0x1.abf8c29cb5e1fp-82, -0x1.b878f169701fp-139 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 16 - i;
      eoff = 42;
    }
  else if (i > 7)
    {
      static const double ch[]
	  = { 0x1.b693p-1, 0x1.c823p-1, 0x1.e818p-2, 0x1.64d8p-3, 0x1.8fep-5,
	      0x1.6d8p-7,  0x1.1bp-9,	0x1.8p-12,   0x1.cp-15,	  0x1p-17 };
      static const double cl[][2]
	  = { { 0x1.088c57e431647p-19, -0x1.6a4eee248922dp-75 },
	      { 0x1.77c5c5a44df36p-19, 0x1.147a9a47518p-73 },
	      { -0x1.abecfde1c8f8ap-20, 0x1.c281ee34fa516p-74 },
	      { -0x1.92dc5889bed4bp-19, -0x1.c4504cb1edff4p-73 },
	      { 0x1.db335a996d732p-20, 0x1.6a943d5a3ee38p-75 },
	      { 0x1.7d4ad16dfb134p-20, 0x1.b25308daf92e2p-74 },
	      { 0x1.ae97800b1ad43p-19, -0x1.e6e6adfab5a3fp-77 },
	      { -0x1.26020530152b5p-20, -0x1.cb5914ec236edp-75 },
	      { 0x1.6aa2ec6fc1505p-20, -0x1.91abbe5fc37f3p-80 },
	      { -0x1.e955794a75596p-23, -0x1.a281d8cdf613bp-77 },
	      { 0x1.e9b94b4bfa21dp-21, 0x1.70c89842b623cp-75 },
	      { 0x1.bc5163ea8ae61p-24, 0x1.5f2b0982db0e4p-79 },
	      { 0x1.75ca101176aeep-27, -0x1.3d3c8d67b88cdp-81 },
	      { 0x1.256e01de80a6ep-30, 0x1.01efb0d1aa0b1p-86 },
	      { 0x1.b03432c85360dp-34, -0x1.50127b61d0b1bp-88 },
	      { 0x1.2c0022103ca4cp-37, -0x1.57d736f96e2fp-94 },
	      { 0x1.8a17e736845f7p-41, 0x1.052304d4cb211p-95 },
	      { 0x1.eb988d0d47704p-45, 0x1.56226ee7f42a3p-100 },
	      { 0x1.240d0d29d0683p-48, -0x1.e50b4b998bd92p-102 },
	      { 0x1.4b6fc0ddc8c34p-52, 0x1.2eddf425d81f1p-106 },
	      { 0x1.6823099fef7bcp-56, -0x1.bfa709d41ba52p-112 },
	      { 0x1.7781aa77eece5p-60, -0x1.d720fbe73365bp-114 },
	      { 0x1.78751cb3ea19fp-64, -0x1.d54b4d612a2dcp-121 },
	      { 0x1.6b88eb9b19162p-68, 0x1.4849c0581dc04p-123 },
	      { 0x1.52a72ee02366dp-72, 0x1.70c849e5bb724p-128 },
	      { 0x1.310c036e064afp-76, 0x1.8ab237479da58p-131 },
	      { 0x1.0ea2926b068d3p-80, 0x1.5dbacb91566f6p-135 },
	      { 0x1.bd1a2274b9631p-85, -0x1.4e61e72cd6369p-140 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 8 - i;
      eoff = 14;
    }
  else
    {
      static const double ch[]
	  = { 0x1.7437p+0, 0x1.027bp+0, 0x1.9548p-2, 0x1.c558p-4, 0x1.91ap-6,
	      0x1.29p-8,   0x1.7cp-11,	0x1.ap-14,   0x1p-16 };
      static const double cl[][2]
	  = { { 0x1.e7866c657d7e3p-20, -0x1.e572c42467bbep-74 },
	      { -0x1.f681f0f0dd1b2p-19, 0x1.214781d931745p-74 },
	      { 0x1.92b4ede711c8cp-19, -0x1.d65c1a367e3ffp-75 },
	      { -0x1.33d0927c13c96p-21, 0x1.6a79242c63d17p-75 },
	      { -0x1.197f189c3d621p-19, -0x1.4f1a82289c68fp-74 },
	      { -0x1.7452ecb747727p-22, 0x1.6005a038b51acp-76 },
	      { -0x1.51f567dba1e03p-21, 0x1.17ebe5a14f803p-77 },
	      { 0x1.a5b7e360e27cdp-19, -0x1.0546204d2b0e6p-78 },
	      { -0x1.2e38e31f63f02p-19, 0x1.8cc3e369580e9p-75 },
	      { 0x1.944636ddfd4a1p-20, -0x1.cd9abdb0154f7p-74 },
	      { 0x1.58a8314ece20bp-23, 0x1.88bf6bd13a465p-77 },
	      { 0x1.10884c52a7f12p-26, 0x1.bc0b448873f4ap-81 },
	      { 0x1.92952062e7eb5p-30, -0x1.e0b1ce596988ap-86 },
	      { 0x1.172851a4b0c8cp-33, 0x1.a5176b6c6c8fcp-87 },
	      { 0x1.6d6671a30ee9ap-37, 0x1.e8d5ed2fcfc0ep-91 },
	      { 0x1.c4e504af0c1p-41, -0x1.abb82f7703a39p-95 },
	      { 0x1.0b03339bb39cp-44, -0x1.1c99c30af694ap-99 },
	      { 0x1.2bef701d4a3c5p-48, 0x1.fdee26eac0983p-102 },
	      { 0x1.42b6f1065ebcap-52, 0x1.2f930bac677fap-107 },
	      { 0x1.4be839a1e033ap-56, -0x1.88d939a249a33p-110 },
	      { 0x1.498c43c164711p-60, -0x1.23998cec39776p-114 },
	      { 0x1.382547c29f33ap-64, -0x1.4738c89c6e5aep-118 },
	      { 0x1.21e8ce6614723p-68, -0x1.77e521e4ba2dfp-122 },
	      { 0x1.f7692bdf0619cp-73, 0x1.d7caee4bdb637p-127 },
	      { 0x1.cc5367db7cbfcp-77, 0x1.5edbeed0867bep-131 },
	      { 0x1.514eb3bdaa622p-81, -0x1.2d38f68d85701p-136 } };
      fh = poly3 (d, array_length (ch), ch, array_length (cl), cl, &fl);
      jm = 4 - i;
      eoff = 3;
    }

  double wh = 1, wl = 0;
  if (jm)
    {
      if (jm > 0)
	{
	  double xph = x, xpl = 0;
	  wh = xph;
	  for (int j = jm - 1; j; j--)
	    {
	      double l;
	      if (fabs (xph) > 1)
		xph = fasttwosum (xph, 1, &l);
	      else
		xph = fasttwosum (1, xph, &l);
	      xpl += l;
	      xph = fasttwosum (xph, xpl, &xpl);
	      wh = muldd3 (xph, xpl, wh, wl, &wl);
	      if (fabs (wh) > 0x1p518)
		{
		  wh *= 0x1p-500;
		  wl *= 0x1p-500;
		  eoff -= 500;
		}
	    }
	}
      else
	{
	  double xph = x - 1, xpl = 0;
	  wh = xph;
	  for (int j = -1 - jm; j; j--)
	    {
	      double l;
	      xph = fasttwosum (xph, -1, &l);
	      xpl += l;
	      xph = fasttwosum (xph, xpl, &xpl);
	      wh = muldd3 (xph, xpl, wh, wl, &wl);
	    }
	}
    }

  if (jm > 0)
    {
      double rh = 1.0 / wh, rl = (fma (rh, -wh, 1.0) - wl * rh) * rh;
      fh = muldd3 (fh, fl, rh, rl, &fl);
    }
  else if (jm < 0)
    {
      fh = muldd3 (fh, fl, wh, wl, &fl);
    }

  double crr = 0;
  if (jm <= 0)
    {
      crr = ((0x1p-54 + 0x1p-107) - 0x1p-54)
	    + ((0x1p-53 - 0x1p-107) - 0x1p-53);
      fl += fh * (jm * crr);
    }
  else
    {
      double op = 0x1p-53 - 0x1p-107, om = -0x1p-53 + 0x1p-107;
      if (op == -om)
	crr = 0x1p-53 - op;
      fl -= fh * ((jm - 5) * crr * 1.04);
    }

  double eps = 0x1.ep-103 * fh, ub = fh + (fl + eps), lb = fh + (fl - eps);
  uint64_t res = asuint64 (fh + fl);
  int64_t re = (res >> 52) & 0x7ff;
  if (re + eoff <= 0)
    { // subnormal case
      res -= (uint64_t) (eoff + re - 1) << 52;
      res &= UINT64_C(0xfff) << 52;
      double l;
      fh = fasttwosum (asdouble (res), fh, &l);
      fl += l;
      res = asuint64 (fh + fl);
      res &= ~(UINT64_C (0x7ff) << 52);
      return __math_uflow_value (asdouble (res));
    }
  else
    {
      res += (uint64_t) eoff << 52;
    }
  if (ub != lb)
    return as_tgamma_database (x, asdouble (res));
  return asdouble (res);
}

double
__ieee754_gamma_r (double x, int *signgamp)
{
  /* The implementation always returns a correct result, so there is no need
     to adjust the sign.  */
  *signgamp = 0;

  uint64_t t = asuint64 (x);
  uint64_t ax = t << 1;
  if (__glibc_unlikely (ax >= (UINT64_C (0x7ff) << 53)))
    { /* x=NaN or +/-Inf */
      if (ax == (UINT64_C (0x7ff)<< 53))
	{	       /* x=+/-Inf */
	  if (t >> 63) /* x=-Inf */
	    return __math_invalid (x);
	  return x; /* x=+Inf */
	}
      return x + x; /* x=NaN, where x+x ensures the "Invalid operation"
		       exception is set if x is sNaN, and it yields a qNaN */
    }

  double z = x;
  if (__glibc_unlikely (fabs (x) < 0.25))
    { /* |x| < 0x1p-2 */
      if (ax < 0x71e0000000000000ul)
	{ // |x| < 0x1p-112
	  double r;
	  // deal separately with x=2^-1024 to avoid a spurious overflow in 1/x
	  if (x == 0x1p-1024)
	    {
	      r = 0x1.fffffffffffffp+1023 + 0x1p+970;
	      return __math_check_uflow_lt (r, 0x1.fffffffffffffp+1023);
	    }
	  r = 1 / x;
	  if (x == 0)
	    return __math_erange (r);
	  // the following raises the inexact flag in case x=2^k
	  if (__glibc_unlikely (fma (r, x, -1.0) == 0))
	    r -= 0.5;
	  /* gamma(x) ~ 1/x - euler_gamma near x=0, thus we should raise the
	     inexact exception even for x = 2^k.
	     More precisely gamma(x) overflows:
	     * for |x| < 2^-1024 and all rounding modes
	     * for x=-2^-1024 and all rounding modes
	     (the case x=2^-1024 was treated separately above) */
	  if (fabs (x) < 0x1p-1024 || x == -0x1p-1024)
	    return __math_erange (r);
	  return r;
	}
      static const double cc[][2]
	  = { { -0x1.2788cfc6fb619p-1, 0x1.66d81dd231575p-58 },
	      { 0x1.fa658c23b1578p-1, 0x1.dded15c22e35ep-56 },
	      { -0x1.d0a118f324b6p-1, -0x1.bb37df476a7ccp-55 },
	      { 0x1.f6a51055097c6p-1, -0x1.30eee7e7c5482p-55 },
	      { -0x1.f6c80ec38bc47p-1, -0x1.22885891ee90dp-56 } };
      static const double c[] = { 0x1.fc7e0a6e9c2c9p-1, -0x1.fdf3f15764246p-1,
				  0x1.ff07b5af9892cp-1, -0x1.ff803d8f584c4p-1,
				  0x1.ffc07f59b072bp-1, -0x1.ffe00e422ee2ep-1,
				  0x1.fff102b561602p-1, -0x1.fff9cb7b72f3bp-1,
				  0x1.ffdcb35bbec92p-1, -0x1.ffcc551b96878p-1,
				  0x1.013dde0ace169p+0, -0x1.01baffd0f7e86p+0,
				  0x1.e15c8c643ed7ap-1, -0x1.da0418fdfaac3p-1,
				  0x1.665b8c5abe55p+0,	-0x1.721c7bc0d07cp+0 };
      double x2 = x * x, x4 = x2 * x2, x8 = x4 * x4;
      double c0 = c[0] + x * c[1] + x2 * (c[2] + x * c[3]);
      double c4 = c[4] + x * c[5] + x2 * (c[6] + x * c[7]);
      double c8 = c[8] + x * c[9] + x2 * (c[10] + x * c[11]);
      double c12 = c[12] + x * c[13] + x2 * (c[14] + x * c[15]);
      c0 += x4 * c4;
      c8 += x4 * c12;
      double cl = x * (c0 + x8 * c8);
      double ch = polyddd (x, 5, cc, &cl);
      double fh = 1.0 / z, fl = fma (fh, -z, 1.0) * fh;
      fh = fastsum (fh, fl, ch, cl, &fl);
      double eps = fh * (3.5e-19 + (x2 * x4) * 4e-15);
      double ub = fh + (fl + eps), lb = fh + (fl - eps);
      if (ub != lb)
	return as_tgamma_accurate (x);
      return ub;
    }

  if (__glibc_unlikely (x >= 0x1.573fae561f648p+7))
    return __math_erange (0x1.fp1023 + 0x1.fp1023);

  double fx = floor (x);
  /* compute k only after the overflow check, otherwise the cast to integer
     might overflow */
  int64_t k = fx;
  if (__glibc_unlikely (fx == x))
    { /* x is integer */
      if (x == 0.0f)
	return __math_divzero (0);
      if (x < 0.0f)
	return __math_invalid (0);
      double t0h = 1, t0l = 0, x0 = 1;
      for (int i = 1; i < k; i++, x0 += 1.0)
	t0h = mulddd2 (x0, t0h, t0l, &t0l);
      return t0h + t0l;
    }

  if (__glibc_unlikely (x <= -184.0))
    { /* negative non-integer */
      /* For x <= -184, x non-integer, |gamma(x)| < 2^-1078.  */
      static const double sgn[2] = { 0x1p-1022, -0x1p-1022 };
      return __math_erange (0x1p-1022 * sgn[k & 1]);
    }

  if (x < -3)
    {
      double ll, lh = fasttwosum (-x, 1, &ll);
      lh = as_lgamma_asym (lh, &ll);
      int e;
      lh = as_expd (lh, &ll, &e);
      double ix = floor (x), dx = x - ix;
      int ip = ix;
      double sl, sh = as_sinpid (dx, &sl);
      lh = muldd2 (sh, sl, lh, ll, &ll);
      const double pih = 0x1.921fb54442d18p+1, pil = 0x1.1a62633145c07p-53;
      double rcp = 1 / lh, rh = rcp * pih,
	     rl = rcp * (pil - ll * rh - fma (rh, lh, -pih));
      if (ip & 1)
	{
	  rh = -rh;
	  rl = -rl;
	}
      double eps = rh * (0x1.eb2049057bc61p-68 - x * 0x1.61019f74442b7p-73);
      uint64_t th;
      if (__glibc_likely (ip >= -170))
	{
	  double ub = rh + (rl + eps), lb = rh + (rl - eps);
	  if (ub != lb)
	    return as_tgamma_accurate (x);
	  th = asuint64 (ub);
	  th -= (int64_t) e << 52;
	}
      else
	{
	  th = asuint64 (rh);
	  int re = (th >> 52) & 0x7ff;
	  if (re - e <= 0)
	    {					  // subnormal case
	      th += (int64_t) (e - re + 1) << 52; // 1 <= e-re+1 <= 53
	      th &= UINT64_C (0xfff) << 52;
	      double l;
	      rh = fasttwosum (asdouble (th), rh, &l);
	      rl += l;
	      double ub = rh + (rl + eps), lb = rh + (rl - eps);
	      if (ub != lb)
		return as_tgamma_accurate (x);
	      th = asuint64 (ub);
	      th &= ~(UINT64_C (0x7ff) << 52); // make subnormal
	      return __math_uflow_value (asdouble (th));
	    }
	  else
	    {
	      double ub = rh + (rl + eps), lb = rh + (rl - eps);
	      if (ub != lb)
		return as_tgamma_accurate (x);
	      th = asuint64 (ub);
	      th -= (int64_t) e << 52;
	    }
	}
      return asdouble (th);
    }

  if (x > 4)
    {
      double ll = 0, lh = as_lgamma_asym (x, &ll);
      int e;
      lh = as_expd (lh, &ll, &e);
      double eps = lh * (0x1.2e3b40a0e9b4fp-69 + x * 0x1.6aad80c11872cp-73);
      double ub = lh + (ll + eps), lb = lh + (ll - eps);
      if (ub != lb)
	return as_tgamma_accurate (x);
      uint64_t th = asuint64 (ub);
      th += (int64_t) e << 52;
      return asdouble (th);
    }

  static const double cc[][2]
      = { { 0x1.a96390899a074p+1, -0x1.6e95430fab07p-58 },
	  { 0x1.d545472146024p+1, 0x1.c07f9774e12b3p-56 },
	  { 0x1.491ad1cb98836p+1, 0x1.51e26c4cfd792p-53 },
	  { 0x1.4a0b6a8230929p+0, 0x1.c1c6993b10594p-54 },
	  { 0x1.0e5d232b95859p-1, 0x1.d4248748dd78bp-56 },
	  { 0x1.71d1672129feep-3, 0x1.3b47c61245ee6p-59 },
	  { 0x1.bd2afde7e4816p-5, -0x1.25466b734902dp-60 },
	  { 0x1.d8376e1031a16p-7, 0x1.2cd76af7fbb2p-61 },
	  { 0x1.c9e94992c88c1p-9, 0x1.5d7be78c93d16p-64 },
	  { 0x1.90ba7276a0c19p-11, -0x1.6cad258076bb3p-66 },
	  { 0x1.49cfed9d63c8bp-13, 0x1.0a8ada0cff18dp-74 },
	  { 0x1.ec018849c245bp-16, 0x1.cea7c4e5e9d4fp-70 },
	  { 0x1.65e5a18d31c17p-18, 0x1.12fc2f27069ecp-72 },
	  { 0x1.ca1890add8727p-21, 0x1.69c0fe53eb0fap-75 },
	  { 0x1.378b3b91f9033p-23, -0x1.d62590f524392p-78 },
	  { 0x1.432cdb3640fcap-26, -0x1.33987f0b3b6b6p-81 },
	  { 0x1.f239fc9cf2155p-29, -0x1.8a95d04bfb2e4p-83 },
	  { 0x1.e3ea4e1366932p-33, -0x1.5c950f5465458p-93 } };
  static const double c[] = {
    0x1.a96390899a074p+1,  0x1.d545472146024p+1,  0x1.491ad1cb98836p+1,
    0x1.4a0b6a8230929p+0,  0x1.0e5d232b95859p-1,  0x1.71d1672129feep-3,
    0x1.bd2afde7e4816p-5,  0x1.d8376e1031a16p-7,  0x1.c9e94992c88c1p-9,
    0x1.90ba7276a0c19p-11, 0x1.49cfed9d63c8bp-13, 0x1.ec018849c245bp-16,
    0x1.65e5a18d31c17p-18, 0x1.ca1890add8727p-21, 0x1.378b3b91f9033p-23,
    0x1.432cdb3640fcap-26, 0x1.f239fc9cf2155p-29, 0x1.e3ea4e1366932p-33
  };
  double m = z - 0x1.cp+1, i = roundeven_finite (m);
  double d = z - (i + 0x1.cp+1);
  double d2 = d * d, d4 = d2 * d2;
  double fl = d
	      * ((c[10] + d * c[11]) + d2 * (c[12] + d * c[13])
		 + d4 * ((c[14] + d * c[15]) + d2 * (c[16] + d * c[17])));
  double fh = polyddd (d, 10, cc, &fl);
  int jm = fabs (i);
  double wh = 1, wl = 0;
  double xph = z, xpl = 0;
  if (jm)
    {
      wh = xph;
      for (int j = jm - 1; j; j--)
	{
	  double l;
	  if (fabs (xph) > 1)
	    xph = fasttwosum (xph, 1, &l);
	  else
	    xph = fasttwosum (1, xph, &l);
	  xpl += l;
	  wh = muldd2 (xph, xpl, wh, wl, &wl);
	}
    }
  double rh = 1.0 / wh, rl = (fma (rh, -wh, 1.0) - wl * rh) * rh;
  fh = muldd2 (rh, rl, fh, fl, &fl);
  double eps = fh * 0x1.2e3b40a0e9b4fp-70;
  double ub = fh + (fl + eps), lb = fh + (fl - eps);
  if (ub != lb)
    return as_tgamma_accurate (x);
  return ub;
}
libm_alias_finite (__ieee754_gamma_r, __gamma_r)

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
  int ex = t >> 52, e = ex - 0x3ff;
  t &= ~UINT64_C(0) >> 12;
  double ed = e;
  uint64_t i = t >> (52 - 5);
  int64_t d = t & (~UINT64_C(0) >> 17);
  uint64_t j
      = (t + ((uint64_t) B[i].c0 << 33) + ((int64_t) B[i].c1 * (d >> 16)))
	>> (52 - 10);
  t |= UINT64_C(0x3ff) << 52;
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

static const double st[][2]
    = { { 0x0p+0, 0x0p+0 },
	{ -0x1.b1d63091a013p-64, 0x1.92155f7a3667ep-6 },
	{ -0x1.912bd0d569a9p-61, 0x1.91f65f10dd814p-5 },
	{ -0x1.9a088a8bf6b2cp-59, 0x1.2d52092ce19f6p-4 },
	{ -0x1.e2718d26ed688p-60, 0x1.917a6bc29b42cp-4 },
	{ 0x1.a2704729ae56dp-59, 0x1.f564e56a9730ep-4 },
	{ 0x1.13000a89a11ep-58, 0x1.2c8106e8e613ap-3 },
	{ 0x1.531ff779ddac6p-57, 0x1.5e214448b3fc6p-3 },
	{ -0x1.26d19b9ff8d82p-57, 0x1.8f8b83c69a60bp-3 },
	{ -0x1.af1439e521935p-62, 0x1.c0b826a7e4f63p-3 },
	{ -0x1.42deef11da2c4p-57, 0x1.f19f97b215f1bp-3 },
	{ 0x1.824c20ab7aa9ap-56, 0x1.111d262b1f677p-2 },
	{ -0x1.5d28da2c4612dp-56, 0x1.294062ed59f06p-2 },
	{ 0x1.0c97c4afa2518p-56, 0x1.4135c94176601p-2 },
	{ -0x1.efdc0d58cf62p-62, 0x1.58f9a75ab1fddp-2 },
	{ -0x1.44b19e0864c5dp-56, 0x1.7088530fa459fp-2 },
	{ -0x1.72cedd3d5a61p-57, 0x1.87de2a6aea963p-2 },
	{ 0x1.6da81290bdbabp-57, 0x1.9ef7943a8ed8ap-2 },
	{ 0x1.5b362cb974183p-57, 0x1.b5d1009e15ccp-2 },
	{ 0x1.6850e59c37f8fp-58, 0x1.cc66e9931c45ep-2 },
	{ 0x1.e0d891d3c6841p-58, 0x1.e2b5d3806f63bp-2 },
	{ -0x1.2ec1fc1b776b8p-60, 0x1.f8ba4dbf89abap-2 },
	{ -0x1.a5a014347406cp-55, 0x1.073879922ffeep-1 },
	{ -0x1.ef23b69abe4f1p-55, 0x1.11eb3541b4b23p-1 },
	{ 0x1.b25dd267f66p-55, 0x1.1c73b39ae68c8p-1 },
	{ -0x1.5da743ef3770cp-55, 0x1.26d054cdd12dfp-1 },
	{ -0x1.efcc626f74a6fp-57, 0x1.30ff7fce17035p-1 },
	{ 0x1.e3e25e3954964p-56, 0x1.3affa292050b9p-1 },
	{ 0x1.8076a2cfdc6b3p-57, 0x1.44cf325091dd6p-1 },
	{ 0x1.3c293edceb327p-57, 0x1.4e6cabbe3e5e9p-1 },
	{ -0x1.75720992bfbb2p-55, 0x1.57d69348cecap-1 },
	{ -0x1.251b352ff2a37p-56, 0x1.610b7551d2cdfp-1 },
	{ -0x1.bdd3413b26456p-55, 0x1.6a09e667f3bcdp-1 },
	{ 0x1.0d4ef0f1d915cp-55, 0x1.72d0837efff96p-1 },
	{ -0x1.0f537acdf0ad7p-56, 0x1.7b5df226aafafp-1 },
	{ -0x1.6f420f8ea3475p-56, 0x1.83b0e0bff976ep-1 },
	{ -0x1.2c5e12ed1336dp-55, 0x1.8bc806b151741p-1 },
	{ 0x1.3d419a920df0bp-55, 0x1.93a22499263fbp-1 },
	{ -0x1.30ee286712474p-55, 0x1.9b3e047f38741p-1 },
	{ -0x1.128bb015df175p-56, 0x1.a29a7a0462782p-1 },
	{ 0x1.9f630e8b6dac8p-60, 0x1.a9b66290ea1a3p-1 },
	{ -0x1.926da300ffccep-55, 0x1.b090a581502p-1 },
	{ -0x1.bc69f324e6d61p-55, 0x1.b728345196e3ep-1 },
	{ -0x1.825a732ac700ap-55, 0x1.bd7c0ac6f952ap-1 },
	{ -0x1.6e0b1757c8d07p-56, 0x1.c38b2f180bdb1p-1 },
	{ -0x1.2fb761e946603p-58, 0x1.c954b213411f5p-1 },
	{ -0x1.e7b6bb5ab58aep-58, 0x1.ced7af43cc773p-1 },
	{ -0x1.4ef5295d25af2p-55, 0x1.d4134d14dc93ap-1 },
	{ 0x1.457e610231ac2p-56, 0x1.d906bcf328d46p-1 },
	{ 0x1.83c37c6107db3p-55, 0x1.ddb13b6ccc23cp-1 },
	{ -0x1.014c76c126527p-55, 0x1.e212104f686e5p-1 },
	{ -0x1.16b56f2847754p-57, 0x1.e6288ec48e112p-1 },
	{ 0x1.760b1e2e3f81ep-55, 0x1.e9f4156c62ddap-1 },
	{ 0x1.e82c791f59cc2p-56, 0x1.ed740e7684963p-1 },
	{ 0x1.52c7adc6b4989p-56, 0x1.f0a7efb9230d7p-1 },
	{ -0x1.d7bafb51f72e6p-56, 0x1.f38f3ac64e589p-1 },
	{ 0x1.562172a361fd3p-56, 0x1.f6297cff75cbp-1 },
	{ 0x1.ab256778ffcb6p-56, 0x1.f8764fa714ba9p-1 },
	{ -0x1.7a0a8ca13571fp-55, 0x1.fa7557f08a517p-1 },
	{ 0x1.1ec8668ecaceep-55, 0x1.fc26470e19fd3p-1 },
	{ -0x1.87df6378811c7p-55, 0x1.fd88da3d12526p-1 },
	{ 0x1.521ecd0c67e35p-57, 0x1.fe9cdad01883ap-1 },
	{ -0x1.c57bc2e24aa15p-57, 0x1.ff621e3796d7ep-1 },
	{ -0x1.1354d4556e4cbp-55, 0x1.ffd886084cd0dp-1 },
	{ 0x0p+0, 0x1p+0 } };

static double
as_sinpid (double x, double *l)
{
  x -= 0.5;
  x = fabs (x);
  x *= 128;
  double ix = roundeven_finite (x), d = ix - x, d2 = d * d;
  int ky = ix, kx = 64 - ky;

  double sh = st[kx][1], sl = st[kx][0];
  double ch = st[ky][1], cl = st[ky][0];
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

static const double E0[][2]
    = { { 0x0p+0, 0x1p+0 },
	{ 0x1.d73e2a475b465p-55, 0x1.059b0d3158574p+0 },
	{ 0x1.8a62e4adc610bp-54, 0x1.0b5586cf9890fp+0 },
	{ -0x1.6c51039449b3ap-54, 0x1.11301d0125b51p+0 },
	{ -0x1.19041b9d78a76p-55, 0x1.172b83c7d517bp+0 },
	{ 0x1.e016e00a2643cp-54, 0x1.1d4873168b9aap+0 },
	{ 0x1.9b07eb6c70573p-54, 0x1.2387a6e756238p+0 },
	{ 0x1.612e8afad1255p-55, 0x1.29e9df51fdee1p+0 },
	{ 0x1.6f46ad23182e4p-55, 0x1.306fe0a31b715p+0 },
	{ -0x1.63aeabf42eae2p-54, 0x1.371a7373aa9cbp+0 },
	{ 0x1.ada0911f09ebcp-55, 0x1.3dea64c123422p+0 },
	{ 0x1.89b7a04ef80dp-59, 0x1.44e086061892dp+0 },
	{ 0x1.d4397afec42e2p-56, 0x1.4bfdad5362a27p+0 },
	{ -0x1.07abe1db13cadp-55, 0x1.5342b569d4f82p+0 },
	{ 0x1.6324c054647adp-54, 0x1.5ab07dd485429p+0 },
	{ -0x1.383c17e40b497p-54, 0x1.6247eb03a5585p+0 },
	{ -0x1.bdd3413b26456p-54, 0x1.6a09e667f3bcdp+0 },
	{ -0x1.16e4786887a99p-55, 0x1.71f75e8ec5f74p+0 },
	{ -0x1.41577ee04992fp-55, 0x1.7a11473eb0187p+0 },
	{ -0x1.d4c1dd41532d8p-54, 0x1.82589994cce13p+0 },
	{ 0x1.6e9f156864b27p-54, 0x1.8ace5422aa0dbp+0 },
	{ -0x1.75fc781b57ebcp-57, 0x1.93737b0cdc5e5p+0 },
	{ 0x1.c7c46b071f2bep-56, 0x1.9c49182a3f09p+0 },
	{ -0x1.d2f6edb8d41e1p-54, 0x1.a5503b23e255dp+0 },
	{ 0x1.7a1cd345dcc81p-54, 0x1.ae89f995ad3adp+0 },
	{ -0x1.5584f7e54ac3bp-56, 0x1.b7f76f2fb5e47p+0 },
	{ 0x1.11065895048ddp-55, 0x1.c199bdd85529cp+0 },
	{ 0x1.503cbd1e949dbp-56, 0x1.cb720dcef9069p+0 },
	{ 0x1.2ed02d75b3707p-55, 0x1.d5818dcfba487p+0 },
	{ -0x1.1a5cd4f184b5cp-54, 0x1.dfc97337b9b5fp+0 },
	{ -0x1.e9c23179c2893p-54, 0x1.ea4afa2a490dap+0 },
	{ 0x1.9d3e12dd8a18bp-54, 0x1.f50765b6e454p+0 } };
static const double E1[][2]
    = { { 0x0p+0, 0x1p+0 },
	{ -0x1.d7c96f201bb2fp-55, 0x1.002c605e2e8cfp+0 },
	{ -0x1.5e00e62d6b30dp-56, 0x1.0058c86da1c0ap+0 },
	{ 0x1.da93f90835f75p-56, 0x1.0085382faef83p+0 },
	{ -0x1.4f6b2a7609f71p-55, 0x1.00b1afa5abcbfp+0 },
	{ -0x1.406ac4e81a645p-57, 0x1.00de2ed0ee0f5p+0 },
	{ 0x1.c1d0660524e08p-54, 0x1.010ab5b2cbd11p+0 },
	{ -0x1.2b6aeb6176892p-56, 0x1.0137444c9b5b5p+0 },
	{ 0x1.b61299ab8cdb7p-54, 0x1.0163da9fb3335p+0 },
	{ -0x1.008eff5142bf9p-56, 0x1.019078ad6a19fp+0 },
	{ 0x1.5e7626621eb5bp-56, 0x1.01bd1e77170b4p+0 },
	{ -0x1.c11f5239bf535p-55, 0x1.01e9cbfe113efp+0 },
	{ -0x1.2bf310fc54eb6p-55, 0x1.02168143b0281p+0 },
	{ -0x1.314aa16278aa3p-54, 0x1.02433e494b755p+0 },
	{ -0x1.082ef51b61d7ep-56, 0x1.027003103b10ep+0 },
	{ 0x1.64cbba902ca27p-58, 0x1.029ccf99d720ap+0 },
	{ -0x1.19083535b085dp-56, 0x1.02c9a3e778061p+0 },
	{ -0x1.b8db0e9dbd87ep-55, 0x1.02f67ffa765e6p+0 },
	{ 0x1.fea8d61ed6016p-54, 0x1.032363d42b027p+0 },
	{ 0x1.bc2ee8e5799acp-54, 0x1.03504f75ef071p+0 },
	{ 0x1.56811eeade11ap-57, 0x1.037d42e11bbccp+0 },
	{ -0x1.f1a93c1b824d3p-54, 0x1.03aa3e170aafep+0 },
	{ 0x1.b7c00e7b751dap-54, 0x1.03d7411915a8ap+0 },
	{ 0x1.9dc3add8f9c02p-54, 0x1.04044be896ab6p+0 },
	{ -0x1.0a31c1977c96ep-54, 0x1.04315e86e7f85p+0 },
	{ 0x1.35bc86af4ee9ap-56, 0x1.045e78f5640b9p+0 },
	{ 0x1.21cd53d5e8b66p-57, 0x1.048b9b35659d8p+0 },
	{ -0x1.e7992580447bp-56, 0x1.04b8c54847a28p+0 },
	{ 0x1.4c3793aa0d08dp-55, 0x1.04e5f72f654b1p+0 },
	{ 0x1.79a8be239ca45p-54, 0x1.051330ec1a03fp+0 },
	{ -0x1.abcae24b819dfp-54, 0x1.0540727fc1762p+0 },
	{ 0x1.06c87433776c9p-55, 0x1.056dbbebb786bp+0 } };

static double
as_expd (double x, double *l, int *e)
{
  const double ln2h = 0x1.71547652b82fep+10, ln2l = 0x1.777d0ffda0d24p-46;
  double xh = x, xl = *l;
  xh = muldd2 (xh, xl, ln2h, ln2l, &xl);
  double ix = roundeven_finite (xh);
  xh = fasttwosum (xh - ix, xl, &xl);
  int k = ix, i0 = (k >> 5) & 31, i1 = k & 31;
  *e = k >> 10;
  double rl, rh = muldd2 (E0[i0][1], E0[i0][0], E1[i1][1], E1[i1][0], &rl);
  static const double c[][2]
      = { { 0x1.62e42fefa39efp-11, 0x1.abc9e3bf9d4d1p-66 },
	  { 0x1.ebfbdff82c58ep-23, 0x1.ec07243b4e585p-77 },
	  { 0x1.c6b08d704a0bfp-35, 0x1.94bac118264d5p-89 },
	  { 0x1.3b2ab719edc2dp-47, 0x1.b530cee32e3dep-101 },
	  { 0x1.5d87fe98a5fc4p-60, -0x1.63e85fdbde1cap-115 } };
  const int m = 1;
  double fh, fl, el;
  fl = xh * polyd (xh, 5 - m, c + m);
  fh = polydd2 (xh, xl, m, c, &fl);
  fh = muldd2 (xh, xl, fh, fl, &fl);
  fh = fasttwosum (1, fh, &el);
  fl += el;
  rh = muldd2 (rh, rl, fh, fl, &rl);
  *l = rl;
  return rh;
}

static double
as_lgamma_asym (double xh, double *xl)
{
  double zh = 1.0 / xh, dz = *xl * zh, zl = (fma (zh, -xh, 1.0) - dz) * zh;
  double ll, lh = as_logd (xh, &ll);
  ll += dz;
  lh = muldd2 (xh - 0.5, *xl, lh - 1, ll, &ll);
  double z2l, z2h = muldd2 (zh, zl, zh, zl, &z2l);
  double fh, fl;
  double x2 = z2h * z2h;
  if (xh > 11.5)
    {
      static const double c[][2]
	  = { { 0x1.acfe390c97d69p-2, 0x1.34acf208a22c4p-56 },
	      { 0x1.5555555555555p-4, 0x1.31799ffbcdddbp-58 },
	      { -0x1.6c16c16c165a9p-9, 0x1.1eefaee02f69p-63 },
	      { 0x1.a01a019ada522p-11, -0x1.4d52971deb155p-66 },
	      { -0x1.381377e3a546dp-11, 0x1.fd1b354a8db62p-65 },
	      { 0x1.b9486dc1c9886p-11, -0x1.2dac4b8cca031p-65 },
	      { -0x1.f3ecd8799f337p-10, 0x1.da5dd745e3963p-64 },
	      { 0x1.6d399e561839p-8, 0x1.15e3000de141ap-62 } };
      lh = fastsum (lh, ll, c[0][0], c[0][1], &ll);
      const int k = 1;
      const double (*b)[2] = c + 1, (*q)[2] = c + 1 + k;
      double q0 = q[0][0] + z2h * q[1][0];
      double q2 = q[2][0] + z2h * q[3][0];
      double q4 = q[4][0] + z2h * q[5][0];
      fl = z2h * (q0 + x2 * (q2 + x2 * q4));
      fh = polydd2 (z2h, z2l, k, b, &fl);
    }
  else
    {
      static const double c[][2]
	  = { { 0x1.acfe390c97d69p-2, 0x1.f06a157d44d5bp-56 },
	      { 0x1.5555555555541p-4, 0x1.9d5fc10df4161p-58 },
	      { -0x1.6c16c16bfb733p-9, -0x1.557d8fba9e97ap-64 },
	      { 0x1.a01a01651819cp-11, -0x1.dd3c0f402122ap-65 },
	      { -0x1.38136b229bfb4p-11, -0x1.879990edddc5fp-67 },
	      { 0x1.b94c0472d00ap-11, 0x1.215a15f7d9289p-65 },
	      { -0x1.f619a122c3918p-10, 0x1.13405abdba76dp-64 },
	      { 0x1.9edef47081644p-8, 0x1.d9d833b12b9bp-62 },
	      { -0x1.bfc20185bf7ccp-6, -0x1.8aa555605e3b1p-60 },
	      { 0x1.0e832a937233p-3, 0x1.871cbde1ab342p-58 },
	      { -0x1.2beb46518ed4ap-1, -0x1.0298c44c99ceep-58 },
	      { 0x1.e5717107e0999p+0, 0x1.5bdfe7ac38f81p-56 },
	      { -0x1.90c04fbd840a6p+1, -0x1.5d2fbfe47e148p-54 } };
      lh = fastsum (lh, ll, c[0][0], c[0][1], &ll);
      double x4 = x2 * x2;
      const int k = 2;
      const double (*b)[2] = c + 1, (*q)[2] = c + 1 + k;
      double q0 = q[0][0] + z2h * q[1][0];
      double q2 = q[2][0] + z2h * q[3][0];
      double q4 = q[4][0] + z2h * q[5][0];
      double q6 = q[6][0] + z2h * q[7][0];
      double q8 = q[8][0] + z2h * q[9][0];
      q4 += x2 * (q6 + x2 * q8);
      q0 += x2 * q2;
      q0 += x4 * q4;
      fl = z2h * q0;
      fh = polydd2 (z2h, z2l, k, b, &fl);
    }
  fh = muldd2 (zh, zl, fh, fl, &fl);
  return fastsum (lh, ll, fh, fl, xl);
}
