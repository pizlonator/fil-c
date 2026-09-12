/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* __ieee754_rem_pio2(x,y)
 *
 * return the remainder of x rem pi/2 in y[0]+y[1]
 * use __kernel_rem_pio2()
 *
 * The callers (sin, cos, sincos, and tan) reduce smaller arguments
 * themselves and handle non-finite inputs, so only the huge-argument
 * path of the original fdlibm routine is kept: the caller guarantees
 * 1e8 < |x| < 2^1024.
 */

#include <math.h>
#include <math_private.h>

/*
 * Table of constants for 2/pi, 396 Hex digits (476 decimal) of 2/pi
 */
static const int32_t two_over_pi[] = {
0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62,
0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
};

static const double
  zero    = 0.00000000000000000000e+00, /* 0x00000000, 0x00000000 */
  two24   = 1.67772160000000000000e+07; /* 0x41700000, 0x00000000 */

int32_t
__ieee754_rem_pio2 (double x, double *y)
{
  double z;
  double tx[3];
  int32_t e0, i, nx, n, ix, hx;
  uint32_t low;

  GET_HIGH_WORD (hx, x);                /* high word of x */
  ix = hx & 0x7fffffff;
  /* set z = scalbn(|x|,ilogb(x)-23) */
  GET_LOW_WORD (low, x);
  e0 = (ix >> 20) - 1046;               /* e0 = ilogb(z)-23; */
  INSERT_WORDS (z, ix - ((int32_t) (e0 << 20)), low);
  for (i = 0; i < 2; i++)
    {
      tx[i] = (double) ((int32_t) (z));
      z = (z - tx[i]) * two24;
    }
  tx[2] = z;
  nx = 3;
  while (tx[nx - 1] == zero)
    nx--;                               /* skip zero term */
  n = __kernel_rem_pio2 (tx, y, e0, nx, 2, two_over_pi);
  if (hx < 0)
    {
      y[0] = -y[0]; y[1] = -y[1]; return -n;
    }
  return n;
}
