/* Test ungetc behavior with malloc failures.
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

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xstdio.h>

static volatile bool fail = false;

/* Induce a malloc failure whenever FAIL is set; we use the __LIBC_MALLOC entry
   point to avoid the other alternative, which is RTLD_NEXT.  */
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

static int
do_test (void)
{
  char *filename = NULL;
  struct stat props = {};
  size_t bufsz = 0;

  create_temp_file ("tst-ungetc-nomem.", &filename);
  if (stat (filename, &props) != 0)
    FAIL_EXIT1 ("Could not get file status: %m\n");

  FILE *fp = fopen (filename, "w");

  /* The libio buffer sizes are the same as block size.  This is to ensure that
     the test runs at the read underflow boundary as well.  */
  bufsz = props.st_blksize + 2;

  char *buf = xmalloc (bufsz);
  memset (buf, 'a', bufsz);

  if (fwrite (buf, sizeof (char), bufsz, fp) != bufsz)
    FAIL_EXIT1 ("fwrite failed: %m\n");
  xfclose (fp);

  /* Begin test.  */
  fp = xfopen (filename, "r");

  while (!feof (fp))
    {
      /* Reset the pushback buffer state.  */
      fseek (fp, 0, SEEK_CUR);

      fail = true;
      /* 1: First ungetc should always succeed, as the standard requires.  */
      TEST_COMPARE (ungetc ('b', fp), 'b');

      /* 2: This will result in resizing, which should fail.  */
      TEST_COMPARE (ungetc ('c', fp), EOF);

      /* 3: Now allow the resizing, which should immediately fill up the buffer
         too, since this allocates only double the current buffer, i.e.
         2-bytes.  */
      fail = false;
      TEST_COMPARE (ungetc ('d', fp), 'd');

      /* 4: And fail again because this again forces an alloc, which fails.  */
      fail = true;
      TEST_COMPARE (ungetc ('e', fp), EOF);

      /* 5: Enable allocations again so that we now get a 4-byte buffer.  Now
         both calls should work.  */
      fail = false;
      TEST_COMPARE (ungetc ('f', fp), 'f');
      fail = true;
      TEST_COMPARE (ungetc ('g', fp), 'g');

      /* Drain out the x's.  */
      TEST_COMPARE (fgetc (fp), 'g');
      TEST_COMPARE (fgetc (fp), 'f');
      TEST_COMPARE (fgetc (fp), 'd');

      /* Finally, drain out the first char we had pushed back, followed by one
	 more char from the stream, if present.  */
      TEST_COMPARE (fgetc (fp), 'b');
      char c = fgetc (fp);
      if (!feof (fp))
	TEST_COMPARE (c, 'a');
    }

  /* Final sanity check before we're done.  */
  TEST_COMPARE (ferror (fp), 0);
  xfclose (fp);

  return 0;
}

#include <support/test-driver.c>
