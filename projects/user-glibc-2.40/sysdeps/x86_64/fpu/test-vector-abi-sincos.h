/* Test for vector sincos/sincosf ABI.
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <math.h>
#include <support/test-driver.h>

/* Since libsupport_nonshared.a is placed before test-libmvec*.o, which
   defines do_test, reference support_test_main here to include it to
   avoid undefined reference to support_test_main.  The libmvec ABI test
   doesn't need other symbols in libsupport_nonshared.a.  */
__typeof (support_test_main) *support_test_main_p = support_test_main;

#define N 1000
LIBMVEC_TYPE x[N], s[N], c[N];
LIBMVEC_TYPE *s_ptrs[N];
LIBMVEC_TYPE *c_ptrs[N];

int
test_vector_abi (void)
{
  int i;
  for(i = 0; i < N; i++)
    {
      x[i] = i / 3;
      s_ptrs[i] = &s[i];
      c_ptrs[i] = &c[i];
    }

#pragma omp simd
  for(i = 0; i < N; i++)
    LIBMVEC_SINCOS (x[i], s_ptrs[i], c_ptrs[i]);

  return 0;
}
