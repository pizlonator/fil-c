/* Test offsets in files being read, in particular with ungetc.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <support/check.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

static volatile bool fail = false;

/* Induce a malloc failure whenever FAIL is set.  */
void *
malloc (size_t sz)
{
  if (fail)
    return NULL;

  static void *(*real_malloc) (size_t);

  if (real_malloc == NULL)
    real_malloc = dlsym (RTLD_NEXT, "malloc");

  return real_malloc (sz);
}

/* The name of the temporary file used by all the tests.  */
static char *filename;

/* st_blksize value for that file, or BUFSIZ if out of range.  */
static int blksize = BUFSIZ;

/* Test data, both written to that file and used as an in-memory
   stream.  */
char test_data[2 * BUFSIZ];

/* Ways to open a test stream for reading (that may use different code
   paths in libio).  */
enum test_open_case
  {
    test_open_fopen,
    test_open_fopen_m,
    test_open_fopen64,
    test_open_fopen64_m,
    test_open_fmemopen,
    test_open_max
  };

static const char *const test_open_case_name[test_open_max] =
  {
    "fopen", "fopen(mmap)", "fopen64", "fopen64(mmap)", "fmemopen"
  };

static FILE *
open_test_stream (enum test_open_case c)
{
  FILE *fp;
  switch (c)
    {
    case test_open_fopen:
      fp = fopen (filename, "r");
      break;

    case test_open_fopen_m:
      fp = fopen (filename, "rm");
      break;

    case test_open_fopen64:
      fp = fopen64 (filename, "r");
      break;

    case test_open_fopen64_m:
      fp = fopen64 (filename, "rm");
      break;

    case test_open_fmemopen:
      fp = fmemopen (test_data, 2 * BUFSIZ, "r");
      break;

    default:
      abort ();
    }
  TEST_VERIFY_EXIT (fp != NULL);
  return fp;
}

/* Base locations at which the main test (ungetc calls then doing
   something that clears ungetc characters, then checking offset)
   starts.  */
enum test_base_loc
  {
    base_loc_start,
    base_loc_blksize,
    base_loc_half,
    base_loc_bufsiz,
    base_loc_eof,
    base_loc_max
  };

static int
base_loc_to_bytes (enum test_base_loc loc, int offset)
{
  switch (loc)
    {
    case base_loc_start:
      return offset;

    case base_loc_blksize:
      return blksize + offset;

    case base_loc_half:
      return BUFSIZ / 2 + offset;

    case base_loc_bufsiz:
      return BUFSIZ + offset;

    case base_loc_eof:
      return 2 * BUFSIZ + offset;

    default:
      abort ();
    }
}

/* Ways to clear data from ungetc.  */
enum clear_ungetc_case
  {
    clear_fseek,
    clear_fseekm1,
    clear_fseekp1,
    clear_fseeko,
    clear_fseekom1,
    clear_fseekop1,
    clear_fseeko64,
    clear_fseeko64m1,
    clear_fseeko64p1,
    clear_fsetpos,
    clear_fsetposu,
    clear_fsetpos64,
    clear_fsetpos64u,
    clear_fflush,
    clear_fflush_null,
    clear_fclose,
    clear_max
  };

static const char *const clear_ungetc_case_name[clear_max] =
  {
    "fseek", "fseek(-1)", "fseek(1)", "fseeko", "fseeko(-1)", "fseeko(1)",
    "fseeko64", "fseeko64(-1)", "fseeko64(1)", "fsetpos", "fsetpos(before)",
    "fsetpos64", "fsetpos64(before)", "fflush", "fflush(NULL)", "fclose"
  };

static int
clear_offset (enum clear_ungetc_case c, int num_ungetc)
{
  switch (c)
    {
    case clear_fseekm1:
    case clear_fseekom1:
    case clear_fseeko64m1:
      return -1;

    case clear_fseekp1:
    case clear_fseekop1:
    case clear_fseeko64p1:
      return 1;

    case clear_fsetposu:
    case clear_fsetpos64u:
      return num_ungetc;

    default:
      return 0;
    }
}

/* The offsets used with fsetpos / fsetpos64.  */
static fpos_t pos;
static fpos64_t pos64;

static int
do_clear_ungetc (FILE *fp, enum clear_ungetc_case c, int num_ungetc)
{
  int ret;
  int offset = clear_offset (c, num_ungetc);
  switch (c)
    {
    case clear_fseek:
    case clear_fseekm1:
    case clear_fseekp1:
      ret = fseek (fp, offset, SEEK_CUR);
      break;

    case clear_fseeko:
    case clear_fseekom1:
    case clear_fseekop1:
      ret = fseeko (fp, offset, SEEK_CUR);
      break;

    case clear_fseeko64:
    case clear_fseeko64m1:
    case clear_fseeko64p1:
      ret = fseeko64 (fp, offset, SEEK_CUR);
      break;

    case clear_fsetpos:
    case clear_fsetposu:
      ret = fsetpos (fp, &pos);
      break;

    case clear_fsetpos64:
    case clear_fsetpos64u:
      ret = fsetpos64 (fp, &pos64);
      break;

    case clear_fflush:
      ret = fflush (fp);
      break;

    case clear_fflush_null:
      ret = fflush (NULL);
      break;

    case clear_fclose:
      ret = fclose (fp);
      break;

    default:
      abort();
    }
  TEST_COMPARE (ret, 0);
  return offset;
}

static bool
clear_valid (enum test_open_case c, enum clear_ungetc_case cl)
{
  switch (c)
    {
    case test_open_fmemopen:
      /* fflush is not valid for input memory streams, and fclose is
	 useless for this test for such streams because there is no
	 underlying open file description for which an offset could be
	 checked after fclose.  */
      switch (cl)
	{
	case clear_fflush:
	case clear_fflush_null:
	case clear_fclose:
	  return false;

	default:
	  return true;
	}

    default:
      /* All ways of clearing ungetc state are valid for streams with
	 an underlying file.  */
      return true;
    }
}

static bool
clear_closes_file (enum clear_ungetc_case cl)
{
  switch (cl)
    {
    case clear_fclose:
      return true;

    default:
      return false;
    }
}

static void
clear_getpos_before (FILE *fp, enum clear_ungetc_case c)
{
  switch (c)
    {
    case clear_fsetposu:
      TEST_COMPARE (fgetpos (fp, &pos), 0);
      break;

    case clear_fsetpos64u:
      TEST_COMPARE (fgetpos64 (fp, &pos64), 0);
      break;

    default:
      break;
    }
}

static void
clear_getpos_after (FILE *fp, enum clear_ungetc_case c)
{
  switch (c)
    {
    case clear_fsetpos:
      TEST_COMPARE (fgetpos (fp, &pos), 0);
      break;

    case clear_fsetpos64:
      TEST_COMPARE (fgetpos64 (fp, &pos64), 0);
      break;

    default:
      break;
    }
}

/* Ways to verify results of clearing ungetc data.  */
enum verify_case
  {
    verify_read,
    verify_ftell,
    verify_ftello,
    verify_ftello64,
    verify_fd,
    verify_max
  };

static const char *const verify_case_name[verify_max] =
  {
    "read", "ftell", "ftello", "ftello64", "fd"
  };

static bool
valid_fd_offset (enum test_open_case c, enum clear_ungetc_case cl)
{
  switch (c)
    {
    case test_open_fmemopen:
      /* No open file description.  */
      return false;

    default:
      /* fseek does not necessarily set the offset for the underlying
	 open file description ("If the most recent operation, other
	 than ftell(), on a given stream is fflush(), the file offset
	 in the underlying open file description shall be adjusted to
	 reflect the location specified by fseek()." in POSIX does not
	 include the case here where getc was the last operation).
	 Similarly, fsetpos does not necessarily set that offset
	 either.  */
      switch (cl)
	{
	case clear_fflush:
	case clear_fflush_null:
	case clear_fclose:
	  return true;

	default:
	  return false;
	}
    }
}

static bool
verify_valid (enum test_open_case c, enum clear_ungetc_case cl,
	      enum verify_case v)
{
  switch (v)
    {
    case verify_fd:
      return valid_fd_offset (c, cl);

    default:
      switch (cl)
	{
	case clear_fclose:
	  return false;

	default:
	  return true;
	}
    }
}

static bool
verify_uses_fd (enum verify_case v)
{
  switch (v)
    {
    case verify_fd:
      return true;

    default:
      return false;
    }
}

static int
read_to_test_loc (FILE *fp, enum test_base_loc loc, int offset)
{
  int to_read = base_loc_to_bytes (loc, offset);
  for (int i = 0; i < to_read; i++)
    TEST_COMPARE (getc (fp), (unsigned char) i);
  return to_read;
}

static void
setup (void)
{
  int fd = create_temp_file ("tst-read-offset", &filename);
  TEST_VERIFY_EXIT (fd != -1);
  struct stat64 st;
  xfstat64 (fd, &st);
  if (st.st_blksize > 0 && st.st_blksize < BUFSIZ)
    blksize = st.st_blksize;
  printf ("BUFSIZ = %d, blksize = %d\n", BUFSIZ, blksize);
  xclose (fd);
  FILE *fp = xfopen (filename, "w");
  for (size_t i = 0; i < 2 * BUFSIZ; i++)
    {
      unsigned char c = i;
      TEST_VERIFY_EXIT (fputc (c, fp) == c);
      test_data[i] = c;
    }
  xfclose (fp);
}

static void
test_one_case (enum test_open_case c, enum test_base_loc loc, int offset,
	       int num_ungetc, int num_ungetc_diff, bool ungetc_fallback,
	       enum clear_ungetc_case cl, enum verify_case v)
{
  int full_offset = base_loc_to_bytes (loc, offset);
  printf ("Testing %s offset %d ungetc %d different %d %s%s %s\n",
	  test_open_case_name[c], full_offset, num_ungetc, num_ungetc_diff,
	  ungetc_fallback ? "fallback " : "", clear_ungetc_case_name[cl],
	  verify_case_name[v]);
  FILE *fp = open_test_stream (c);
  int cur_offset = read_to_test_loc (fp, loc, offset);
  clear_getpos_before (fp, cl);
  for (int i = 0; i < num_ungetc; i++)
    {
      unsigned char c = (i >= num_ungetc - num_ungetc_diff
			 ? cur_offset
			 : cur_offset - 1);
      if (ungetc_fallback)
	fail = true;
      TEST_COMPARE (ungetc (c, fp), c);
      fail = false;
      cur_offset--;
    }
  clear_getpos_after (fp, cl);
  int fd = -1;
  bool done_dup = false;
  if (verify_uses_fd (v))
    {
      fd = fileno (fp);
      TEST_VERIFY (fd != -1);
      if (clear_closes_file (cl))
	{
	  fd = xdup (fd);
	  done_dup = true;
	}
    }
  cur_offset += do_clear_ungetc (fp, cl, num_ungetc);
  switch (v)
    {
    case verify_read:
      for (;
	   cur_offset <= full_offset + 1 && cur_offset < 2 * BUFSIZ;
	   cur_offset++)
	TEST_COMPARE (getc (fp), (unsigned char) cur_offset);
      break;

    case verify_ftell:
      TEST_COMPARE (ftell (fp), cur_offset);
      break;

    case verify_ftello:
      TEST_COMPARE (ftello (fp), cur_offset);
      break;

    case verify_ftello64:
      TEST_COMPARE (ftello64 (fp), cur_offset);
      break;

    case verify_fd:
      TEST_COMPARE (lseek (fd, 0, SEEK_CUR), cur_offset);
      break;

    default:
      abort ();
    }
  if (! clear_closes_file (cl))
    {
      int ret = fclose (fp);
      TEST_COMPARE (ret, 0);
    }
  if (done_dup)
    xclose (fd);
}

int
do_test (void)
{
  setup ();
  for (enum test_open_case c = 0; c < test_open_max; c++)
    for (enum test_base_loc loc = 0; loc < base_loc_max; loc++)
      for (int offset = -2; offset <= 3; offset++)
	for (int num_ungetc = 0;
	     num_ungetc <= 2 && num_ungetc <= base_loc_to_bytes (loc, offset);
	     num_ungetc++)
	  for (int num_ungetc_diff = 0;
	       num_ungetc_diff <= num_ungetc;
	       num_ungetc_diff++)
	    for (int ungetc_fallback = 0;
		 ungetc_fallback <= (num_ungetc == 1 ? 1 : 0);
		 ungetc_fallback++)
	      for (enum clear_ungetc_case cl = 0; cl < clear_max; cl++)
		{
		  if (!clear_valid (c, cl))
		    continue;
		  if (base_loc_to_bytes (loc, offset) > 2 * BUFSIZ)
		    continue;
		  if ((base_loc_to_bytes (loc, offset)
		       - num_ungetc
		       + clear_offset (cl, num_ungetc)) < 0)
		    continue;
		  if ((base_loc_to_bytes (loc, offset)
		       - num_ungetc
		       + clear_offset (cl, num_ungetc)) > 2 * BUFSIZ)
		    continue;
		  for (enum verify_case v = 0; v < verify_max; v++)
		    {
		      if (!verify_valid (c, cl, v))
			continue;
		      test_one_case (c, loc, offset, num_ungetc,
				     num_ungetc_diff, ungetc_fallback, cl, v);
		    }
		}
  return 0;
}

#include <support/test-driver.c>
