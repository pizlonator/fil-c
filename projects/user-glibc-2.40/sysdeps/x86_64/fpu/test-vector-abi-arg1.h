/* Test for vector ABI with a single argument.
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
LIBMVEC_TYPE x[N], c[N];

int
test_vector_abi (void)
{
  int i;
  for(i = 0; i < N; i++)
    c[i] = i / 3;

#pragma omp simd
  for(i = 0; i < N; i++)
    x[i] = LIBMVEC_FUNC (c[i]);

  return 0;
}
