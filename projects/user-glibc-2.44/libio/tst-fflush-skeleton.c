/* Test that fflush (FILE) and fflush (NULL) are semantically equivalent.

   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

/* A success on this test doesn't imply the effectiveness of fflush as
   we can't ensure that the file wasn't already in the expected state
   before the call of the function. It only ensures that, if the test
   fails, fflush is broken.  */

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

#define CONTENT_SZ_MAX 32
#define TEST_FILE_COUNT 10

struct file_tracking
{
  FILE *file;
  char *name;
  int fd;
  char *mfile;
} files[TEST_FILE_COUNT];

static void
file_init (int file)
{
  int fd = -1;

  assert (file < TEST_FILE_COUNT);

  files[file] = (struct file_tracking) { .fd = -1, };

  xclose (create_temp_file ("tst-fflush", &files[file].name));

  fd = xopen (files[file].name, O_RDONLY, 0);
  files[file].mfile = xmmap (NULL, CONTENT_SZ_MAX, PROT_READ, MAP_SHARED, fd);
  xclose (fd);
}

static void
file_cleanup (int file)
{
  free (files[file].name);
  xmunmap (files[file].mfile, CONTENT_SZ_MAX);
  files[file] = (struct file_tracking) { .fd = -1, };
}

static void
file_changed (int to_check, const char *mode)
{
  struct stat stats = { };
  char expected[CONTENT_SZ_MAX] = { };

  verbose_printf ("Check that %s (%d) exactly contains the data we put in\n",
		  files[to_check].name, to_check);

  /* File should contain "N:M" where both N and M are one digit exactly.  */
  snprintf (expected, sizeof (expected), "%d:%d", FILE_FLUSH_TYPE, to_check);
  TEST_COMPARE_BLOB (files[to_check].mfile, sizeof (expected),
		     expected, sizeof (expected));

  TEST_VERIFY (fstat (files[to_check].fd, &stats) >= 0);
  TEST_VERIFY (stats.st_size == 3);
  /* In read mode we expect to be at position 1, in write mode at position 3 */
  TEST_COMPARE (lseek (files[to_check].fd, 0, SEEK_CUR),
		mode[0] == 'r' ? 1 : 3);

  if (support_record_failure_is_failed ())
    FAIL_EXIT1 ("exiting due to previous failure");

  /* Not reached if the data doesn't match.  */
}

static void
file_flush (const char *mode)
{
  for (int i = 0; i < TEST_FILE_COUNT; i++)
    {
      files[i].file = xfopen (files[i].name, mode);
      files[i].fd = fileno (files[i].file);
    }

  /* Print a unique identifier in each file, that is not too long nor contain
     new line to not trigger _IO_OVERFLOW/_IO_SYNC.  */
  for (int i = 0; i < TEST_FILE_COUNT; i++)
    {
      if (mode[0] == 'r')
	fgetc (files[i].file);
      else
	fprintf (files[i].file, "%d:%d", FILE_FLUSH_TYPE, i);
    }

  if (!FILE_FLUSH_TYPE)
    TEST_VERIFY (fflush (NULL) == 0);
  else
    for (int i = 0; i < TEST_FILE_COUNT; i++)
      TEST_VERIFY (fflush (files[i].file) == 0);

  for (int i = 0; i < TEST_FILE_COUNT; i++)
    {
      verbose_printf ("Check that file %s has been modified after fflush\n",
		      files[i].name);
      file_changed (i, mode);
    }

  for (int i = 0; i < TEST_FILE_COUNT; i++)
    xfclose (files[i].file);
}

static int
do_test (void)
{
  for (int i = 0; i < TEST_FILE_COUNT; i++)
    file_init (i);

  verbose_printf ("Checking fflush(" S_FLUSH_TYPE "), WRITE mode\n");
  file_flush ("w");

  verbose_printf ("Checking fflush(" S_FLUSH_TYPE "), READWRITE mode\n");
  file_flush ("r+");

  for (int i = 0; i < TEST_FILE_COUNT; i++)
    file_cleanup (i);

  return 0;
}

#include <support/test-driver.c>
