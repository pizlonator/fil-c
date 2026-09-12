/* Benchmark fclose.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bench-timing.h"
#include "json-lib.h"

#define NUM_FILES	1000000
#define NUM_FCLOSE	100

int
main (int argc, char **argv)
{
  json_ctx_t json_ctx;
  json_init (&json_ctx, 0, stdout);
  json_document_begin (&json_ctx);

  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);
  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, "fclose");

  FILE *ff, *keep[NUM_FCLOSE];
  int i;

  for (i = 0; i < NUM_FILES; i++)
    {
      ff = fdopen (STDIN_FILENO, "r");
      if (!ff)
	{
	  fprintf (stderr, "### failed to fdopen: %m\n");
	  return EXIT_FAILURE;
	}
      if (i < NUM_FCLOSE)
	keep[i] = ff;
    }

  timing_t start, stop, elapsed;

  TIMING_NOW (start);

  for (i = 0; i < NUM_FCLOSE; i++)
    fclose (keep[i]);

  TIMING_NOW (stop);

  TIMING_DIFF (elapsed, start, stop);

  json_attr_uint (&json_ctx, "number of FILEs", NUM_FILES);
  json_attr_uint (&json_ctx, "number of fclose calls", NUM_FCLOSE);
  json_attr_uint (&json_ctx, "duration", elapsed);

  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return 0;
}
