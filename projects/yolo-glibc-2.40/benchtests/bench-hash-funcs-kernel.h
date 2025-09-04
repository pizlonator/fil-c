/* Actual benchmark kernels used by bench-hash-funcs.h
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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


#include "bench-util.h"

/* We go through the trouble of using macros here because many of the
   hash functions are meant to be inlined so its not fair to benchmark
   them with a function pointer where they won't be inlinable. */
#undef RUN_FUNC
#undef POSTFIX
#ifdef SIMPLE
# define RUN_FUNC SIMPLE_TEST_FUNC
# define POSTFIX _simple
#else
# define RUN_FUNC TEST_FUNC
# define POSTFIX _optimized
#endif

#define PRIMITIVE_CAT(x, y) x ## y
#define CAT(x, y) PRIMITIVE_CAT (x, y)

static double __attribute__ ((noinline, noclone))
CAT (do_one_test_kernel, POSTFIX) (const char *s, size_t len)
{

  unsigned int iters;
  timing_t start, stop, cur;

  /* Warmup.  */
  for (iters = NFIXED_ITERS / 32; iters; --iters)
    DO_NOT_OPTIMIZE_OUT (RUN_FUNC (s, len));

  TIMING_NOW (start);
  for (iters = NFIXED_ITERS; iters; --iters)
    DO_NOT_OPTIMIZE_OUT (RUN_FUNC (s, len));

  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  (void) (len);
  return (double) cur / (double) NFIXED_ITERS;
}

static double __attribute__ ((noinline, noclone))
CAT (do_rand_test_kernel, POSTFIX) (char const *bufs,
				    unsigned int const *sizes)
{
  unsigned int i, iters;
  size_t offset;
  timing_t start, stop, cur;

  /* Warmup.  */
  for (i = 0, offset = 0; i < NRAND_BUFS; ++i, offset += RAND_BENCH_MAX_LEN)
    DO_NOT_OPTIMIZE_OUT (RUN_FUNC (bufs + offset, sizes[i]));

  TIMING_NOW (start);
  for (iters = NRAND_ITERS; iters; --iters)
    {
      for (i = 0, offset = 0; i < NRAND_BUFS;
	   ++i, offset += RAND_BENCH_MAX_LEN)
	DO_NOT_OPTIMIZE_OUT (RUN_FUNC (bufs + offset, sizes[i]));

    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  (void) (sizes);
  return (double) cur / (double) (NRAND_ITERS * NRAND_BUFS);
}
