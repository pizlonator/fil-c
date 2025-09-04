/* Measure STRCHR functions.
   Copyright (C) 2013-2024 Free Software Foundation, Inc.
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

#define TEST_MAIN
#ifdef WIDE
# define TEST_NAME "wcsrchr"
#else
# define TEST_NAME "strrchr"
#endif
#include "bench-string.h"
#include "json-lib.h"

#define BIG_CHAR MAX_CHAR

#ifdef WIDE
# define SMALL_CHAR 1273
#else
# define SMALL_CHAR 127

char *
generic_strrchr (const char *, int);

IMPL (generic_strrchr, 0)

#endif

typedef CHAR *(*proto_t) (const CHAR *, int);

IMPL (STRRCHR, 1)

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, const CHAR *s, int c,
	     CHAR *exp_res)
{
  CHAR *res = CALL (impl, s, c);
  size_t i, iters = INNER_LOOP_ITERS8;
  timing_t start, stop, cur;

  if (res != exp_res)
    {
      error (0, 0, "Wrong result in function %s %p %p", impl->name, res,
	     exp_res);
      ret = 1;
      return;
    }

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      CALL (impl, s, c);
    }
  TIMING_NOW (stop);
  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double) cur / (double) iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t align, size_t pos, size_t len,
	 int seek_char, int max_char, size_t freq)
/* For wcsrchr: align here means align not in bytes,
   but in wchar_ts, in bytes it will equal to align * (sizeof (wchar_t))
   len for wcschr here isn't in bytes but it's number of wchar_t symbols.  */
{
  size_t i;
  size_t pos_chunk_sz = freq ? (pos / freq) : pos;
  size_t last_pos = len;
  CHAR *result;
  CHAR *buf = (CHAR *) buf1;

  align &= (getpagesize () - 1);
  if ((align + len) * sizeof (CHAR) >= page_size)
    return;

  for (i = 0; i < len; ++i)
    {
      buf[align + i] = (random () * random ()) & max_char;
      if (!buf[align + i])
	buf[align + i] = (random () * random ()) & max_char;
      if (!buf[align + i])
	buf[align + i] = 1;
      if ((i > pos || pos >= len) && buf[align + i] == seek_char)
	buf[align + i] = seek_char + 10 + (random () & 15);
    }

  if (pos_chunk_sz == 0 && pos)
    pos_chunk_sz = 1;

  for (i = pos_chunk_sz; i < pos && i < len; i += pos_chunk_sz)
    {
      buf[align + i] = seek_char;
      last_pos = i;
    }

  buf[align + len] = 0;

  if (pos < len)
    {
      buf[align + pos] = seek_char;
      result = (CHAR *) (buf + align + pos);
    }
  else if (last_pos < len)
    result = (CHAR *) (buf + align + last_pos);
  else if (seek_char == 0)
    result = (CHAR *) (buf + align + len);
  else
    result = NULL;

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "len", len);
  json_attr_uint (json_ctx, "pos", pos);
  json_attr_uint (json_ctx, "align", align);
  json_attr_uint (json_ctx, "freq", freq);
  json_attr_uint (json_ctx, "seek", seek_char);
  json_attr_uint (json_ctx, "max_char", max_char);
  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, (CHAR *) (buf + align), seek_char, result);

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

int
test_main (void)
{
  json_ctx_t json_ctx;
  size_t i, j, k;
  int seek;

  test_init ();
  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);
  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);

  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, TEST_NAME);
  json_attr_string (&json_ctx, "bench-variant", "");

  json_array_begin (&json_ctx, "ifuncs");
  FOR_EACH_IMPL (impl, 0)
    json_element_string (&json_ctx, impl->name);
  json_array_end (&json_ctx);

  json_array_begin (&json_ctx, "results");

  for (seek = 0; seek <= 23; seek += 23)
    {
      for (j = 1; j <= 256; j = (j * 4))
	{
	  for (i = 1; i < 9; ++i)
	    {
	      do_test (&json_ctx, 0, 16 << i, 2048, seek, SMALL_CHAR, j);
	      do_test (&json_ctx, i, 16 << i, 2048, seek, SMALL_CHAR, j);
	    }

	  for (i = 1; i < 8; ++i)
	    {
	      do_test (&json_ctx, i, 64, 256, seek, SMALL_CHAR, j);
	      do_test (&json_ctx, i, 64, 256, seek, BIG_CHAR, j);

	      do_test (&json_ctx, i * 15, 64, 256, seek, SMALL_CHAR, j);
	      do_test (&json_ctx, i * 15, 64, 256, seek, BIG_CHAR, j);
	    }

	  for (i = 0; i < 32; ++i)
	    {
	      do_test (&json_ctx, 0, i, i + 1, seek, SMALL_CHAR, j);
	      do_test (&json_ctx, 0, i, i + 1, seek, BIG_CHAR, j);
	      do_test (&json_ctx, getpagesize () - i / 2 - 1, i, i + 1, seek,
		       SMALL_CHAR, j);
	    }

	  for (i = (16 / sizeof (CHAR)); i <= (288 / sizeof (CHAR)); i += 32)
	    {
	      do_test (&json_ctx, 0, i - 16, i, seek, SMALL_CHAR, j);
	      do_test (&json_ctx, 0, i, i + 16, seek, SMALL_CHAR, j);
	    }

	  for (i = (16 / sizeof (CHAR)); i <= (2048 / sizeof (CHAR)); i += i)
	    {
	      for (k = 0; k <= (288 / sizeof (CHAR));
		   k += (48 / sizeof (CHAR)))
		{
		  do_test (&json_ctx, 0, k, i, seek, SMALL_CHAR, j);
		  do_test (&json_ctx, 0, i, i + k, seek, SMALL_CHAR, j);

		  if (k < i)
		    {
		      do_test (&json_ctx, 0, i - k, i, seek, SMALL_CHAR, j);
		      do_test (&json_ctx, 0, k, i - k, seek, SMALL_CHAR, j);
		      do_test (&json_ctx, 0, i, i - k, seek, SMALL_CHAR, j);
		    }
		}
	    }

	  if (seek == 0)
	    {
	      break;
	    }
	}
    }

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>

#define weak_alias(X,Y)
#define libc_hidden_builtin_def(X)
#ifndef WIDE
# undef STRRCHR
# define STRRCHR generic_strrchr
# define __memrchr memrchr
# include <string/strrchr.c>
#endif
