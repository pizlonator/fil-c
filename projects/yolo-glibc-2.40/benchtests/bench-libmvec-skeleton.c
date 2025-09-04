/* Skeleton for libmvec benchmark programs.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <bench-timing.h>
#include <json-lib.h>
#include <bench-util.h>
#include <math-tests-arch.h>
#include <bench-libmvec-arch.h>

#include <bench-util.c>
#define D_ITERS 200000

int
main (int argc, char **argv)
{
  unsigned long i, k;
  timing_t start, end;
  json_ctx_t json_ctx;

#ifdef INIT_ARCH
  INIT_ARCH ();
#endif

  bench_start ();

#ifdef BENCH_INIT
  BENCH_INIT ();
#endif

  json_init (&json_ctx, 2, stdout);

  /* Begin function.  */
  json_attr_object_begin (&json_ctx, FUNCNAME);

  for (int v = 0; v < NUM_VARIANTS; v++)
    {
      double d_total_time = 0;
      timing_t cur;
      for (k = 0; k < D_ITERS; k++)
	{
	  TIMING_NOW (start);
	  for (i = 0; i < NUM_SAMPLES (v); i++)
	    BENCH_FUNC (v, i);
	  TIMING_NOW (end);

	  TIMING_DIFF (cur, start, end);

	  TIMING_ACCUM (d_total_time, cur);
	}
      double d_total_data_set = D_ITERS * NUM_SAMPLES (v) * STRIDE;

      /* Begin variant.  */
      json_attr_object_begin (&json_ctx, VARIANT (v));

      json_attr_double (&json_ctx, "duration", d_total_time);
      json_attr_double (&json_ctx, "iterations", d_total_data_set);
      json_attr_double (&json_ctx, "mean", d_total_time / d_total_data_set);

      /* End variant.  */
      json_attr_object_end (&json_ctx);
    }

  /* End function.  */
  json_attr_object_end (&json_ctx);

  return 0;
}
