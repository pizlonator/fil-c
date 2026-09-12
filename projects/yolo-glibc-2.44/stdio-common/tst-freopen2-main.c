/* Test freopen.
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

#include <errno.h>
#include <fcntl.h>
#include <mcheck.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <support/check.h>
#include <support/descriptors.h>
#include <support/file_contents.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xstdio.h>

#define START_TEST(DESC)			\
  do						\
    {						\
      fds = support_descriptors_list ();	\
      verbose_printf (DESC);			\
    }						\
  while (0)

#define END_TEST				\
  do						\
    {						\
      support_descriptors_check (fds);		\
      support_descriptors_free (fds);		\
    }						\
  while (0)

int
do_test (void)
{
  mtrace ();
  struct support_descriptors *fds;
  char *temp_dir = support_create_temp_directory ("tst-freopen2");
  char *file1 = xasprintf ("%s/file1", temp_dir);
  support_write_file_string (file1, "file1");
  add_temp_file (file1);
  char *file2 = xasprintf ("%s/file2", temp_dir);
  support_write_file_string (file2, "file2");
  add_temp_file (file2);
  char *file3 = xasprintf ("%s/file3", temp_dir);
  char *file4 = xasprintf ("%s/file4", temp_dir);
  char *file1a = xasprintf ("%s/file1a", temp_dir);
  FILE *fp;
  int ret;
  wint_t wc;

  /* Test each pair of old and new modes from r w a.  */

  START_TEST ("Testing r -> r\n");
  fp = xfopen (file1, "r");
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "file2");
  xfclose (fp);
  END_TEST;

  START_TEST ("Testing r -> w\n");
  fp = xfopen (file1, "r");
  fp = FREOPEN (file2, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("File2new", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file1, "file1");
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "File2new");
  END_TEST;

  START_TEST ("Testing r -> a\n");
  fp = xfopen (file1, "r");
  fp = FREOPEN (file2, "a", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("3", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "File2new3");
  END_TEST;

  START_TEST ("Testing w -> r\n");
  fp = xfopen (file1, "w");
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "File2new3");
  xfclose (fp);
  END_TEST;

  START_TEST ("Testing w -> w\n");
  fp = xfopen (file1, "w");
  fp = FREOPEN (file2, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("next", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file1, "");
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "next");
  END_TEST;

  START_TEST ("Testing w -> a\n");
  fp = xfopen (file1, "w");
  fp = FREOPEN (file2, "a", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("4", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "next4");
  END_TEST;

  START_TEST ("Testing a -> r\n");
  fp = xfopen (file1, "a");
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "next4");
  xfclose (fp);
  END_TEST;

  START_TEST ("Testing a -> w\n");
  fp = xfopen (file1, "a");
  fp = FREOPEN (file2, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("another", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "another");
  END_TEST;

  START_TEST ("Testing a -> a\n");
  fp = xfopen (file1, "a");
  fp = FREOPEN (file2, "a", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("5", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "another5");
  END_TEST;

  /* Test for file originally opened with fopen64.  */
  START_TEST ("Testing fopen64 a -> a\n");
  fp = fopen64 (file1, "a");
  TEST_VERIFY_EXIT (fp != NULL);
  fp = FREOPEN (file2, "a", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("64", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "another564");
  END_TEST;

  /* Test calling freopen more than once on the same FILE *.  */

  START_TEST ("Testing r -> w -> r\n");
  fp = xfopen (file1, "r");
  fp = FREOPEN (file2, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("freopen-twice", fp);
  TEST_VERIFY (ret >= 0);
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "freopen-twice");
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "freopen-twice");
  END_TEST;

  START_TEST ("Testing r -> w -> r (exactly one freopen64)\n");
  fp = xfopen (file1, "r");
  fp = OTHER_FREOPEN (file2, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("freopen-twice64", fp);
  TEST_VERIFY (ret >= 0);
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "freopen-twice64");
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "freopen-twice64");
  END_TEST;

  /* Test changing to/from b (binary, no-op).  */

  START_TEST ("Testing rb -> r\n");
  fp = xfopen (file1, "rb");
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "freopen-twice64");
  xfclose (fp);
  END_TEST;

  START_TEST ("Testing r -> rb\n");
  fp = xfopen (file1, "r");
  fp = FREOPEN (file2, "rb", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "freopen-twice64");
  xfclose (fp);
  END_TEST;

  /* Test changing to/from + (read-and-write).  */

  START_TEST ("Testing r -> w+\n");
  fp = xfopen (file1, "r");
  fp = FREOPEN (file2, "w+", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("latest", fp);
  TEST_VERIFY (ret >= 0);
  ret = fseek (fp, 0, SEEK_SET);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE_FILE_STRING (fp, "latest");
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "latest");
  END_TEST;

  START_TEST ("Testing w -> a+\n");
  fp = xfopen (file1, "w");
  fp = FREOPEN (file2, "a+", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("suffix", fp);
  TEST_VERIFY (ret >= 0);
  ret = fseek (fp, 0, SEEK_SET);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE_FILE_STRING (fp, "latestsuffix");
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "latestsuffix");
  END_TEST;

  START_TEST ("Testing a -> r+\n");
  fp = xfopen (file1, "a");
  fp = FREOPEN (file2, "r+", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "latestsuffix");
  ret = fseek (fp, 0, SEEK_SET);
  TEST_COMPARE (ret, 0);
  ret = fputs ("new", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "newestsuffix");
  END_TEST;

  START_TEST ("Testing r+ -> w\n");
  fp = xfopen (file1, "r+");
  fp = FREOPEN (file2, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("plusto", fp);
  TEST_VERIFY (ret >= 0);
  ret = fseek (fp, 0, SEEK_SET);
  TEST_COMPARE (ret, 0);
  errno = 0;
  TEST_COMPARE (fgetc (fp), EOF);
  TEST_COMPARE (errno, EBADF);
  clearerr (fp);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "plusto");
  END_TEST;

  START_TEST ("Testing w+ -> a\n");
  fp = xfopen (file1, "w+");
  fp = FREOPEN (file2, "a", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("more", fp);
  TEST_VERIFY (ret >= 0);
  ret = fseek (fp, 0, SEEK_SET);
  TEST_COMPARE (ret, 0);
  errno = 0;
  TEST_COMPARE (fgetc (fp), EOF);
  TEST_COMPARE (errno, EBADF);
  clearerr (fp);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "plustomore");
  END_TEST;

  START_TEST ("Testing a+ -> r\n");
  fp = xfopen (file1, "a+");
  fp = FREOPEN (file2, "rr", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "plustomore");
  ret = fputs ("2", fp);
  TEST_COMPARE (ret, EOF);
  clearerr (fp);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "plustomore");
  END_TEST;

  /* Test changing to/from e (FD_CLOEXEC).  */

  START_TEST ("Testing re -> r\n");
  fp = xfopen (file1, "re");
  ret = fcntl (fileno (fp), F_GETFD);
  TEST_VERIFY (ret != -1);
  TEST_COMPARE (ret & FD_CLOEXEC, FD_CLOEXEC);
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fcntl (fileno (fp), F_GETFD);
  TEST_VERIFY (ret != -1);
  TEST_COMPARE (ret & FD_CLOEXEC, 0);
  TEST_COMPARE_FILE_STRING (fp, "plustomore");
  xfclose (fp);
  END_TEST;

  START_TEST ("Testing r -> re\n");
  fp = xfopen (file1, "r");
  ret = fcntl (fileno (fp), F_GETFD);
  TEST_VERIFY (ret != -1);
  TEST_COMPARE (ret & FD_CLOEXEC, 0);
  fp = FREOPEN (file2, "re", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fcntl (fileno (fp), F_GETFD);
  TEST_VERIFY (ret != -1);
  TEST_COMPARE (ret & FD_CLOEXEC, FD_CLOEXEC);
  TEST_COMPARE_FILE_STRING (fp, "plustomore");
  xfclose (fp);
  END_TEST;

  /* Test changing to/from m (mmap) (a no-op as far as testing
     semantics is concerned).  */

  START_TEST ("Testing rm -> r\n");
  fp = xfopen (file1, "rm");
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "plustomore");
  xfclose (fp);
  END_TEST;

  START_TEST ("Testing r -> rm\n");
  fp = xfopen (file1, "r");
  fp = FREOPEN (file2, "rm", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "plustomore");
  xfclose (fp);
  END_TEST;

  /* Test changing to/from x (O_EXCL).  */

  START_TEST ("Testing wx -> w\n");
  fp = xfopen (file3, "wx");
  add_temp_file (file3);
  fp = FREOPEN (file2, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  ret = fputs ("wxtow", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file2, "wxtow");
  END_TEST;

  START_TEST ("Testing w -> wx (file does not exist)\n");
  fp = xfopen (file1, "w");
  fp = FREOPEN (file4, "wx", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  add_temp_file (file4);
  ret = fputs ("wtowx", fp);
  TEST_VERIFY (ret >= 0);
  xfclose (fp);
  TEST_OPEN_AND_COMPARE_FILE_STRING (file4, "wtowx");
  END_TEST;

  /* Test with ,ccs=CHARSET.  */

  START_TEST ("testing w,ccs=utf-8 -> r\n");
  fp = xfopen (file1, "w,ccs=utf-8");
  ret = fputws (L"\xc0\xc1", fp);
  TEST_VERIFY (ret >= 0);
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "wxtow");
  xfclose (fp);
  END_TEST;

  START_TEST ("testing w,ccs=iso-8859-1 -> r,ccs=utf-8\n");
  fp = xfopen (file2, "w,ccs=iso-8859-1");
  ret = fputws (L"\xc0\xc1", fp);
  TEST_VERIFY (ret >= 0);
  fp = FREOPEN (file1, "r,ccs=utf-8", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  wc = fgetwc (fp);
  TEST_COMPARE (wc, (wint_t) 0xc0);
  wc = fgetwc (fp);
  TEST_COMPARE (wc, (wint_t) 0xc1);
  wc = fgetwc (fp);
  TEST_COMPARE (wc, WEOF);
  xfclose (fp);
  END_TEST;

  START_TEST ("testing r,ccs=utf-8 -> r\n");
  fp = xfopen (file1, "r,ccs=utf-8");
  fp = FREOPEN (file1, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "\u00c0\u00c1");
  xfclose (fp);
  END_TEST;

  /* Test that errors closing the old file are ignored.  */

  START_TEST ("testing errors closing old file ignored\n");
  fp = xfopen ("/dev/full", "w");
  fputc ('x', fp);
  fp = FREOPEN (file1, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "\u00c0\u00c1");
  xfclose (fp);
  END_TEST;

  /* Test that error / EOF state from the old file are cleared.  */

  START_TEST ("testing error state from old file cleared\n");
  fp = xfopen ("/dev/full", "w");
  fputc ('x', fp);
  fflush (fp);
  TEST_VERIFY (ferror (fp));
  TEST_VERIFY (!feof (fp));
  fp = FREOPEN (file2, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_VERIFY (!ferror (fp));
  TEST_VERIFY (!feof (fp));
  xfclose (fp);
  END_TEST;

  START_TEST ("testing EOF state from old file cleared\n");
  fp = xfopen ("/dev/null", "r");
  fgetc (fp);
  TEST_VERIFY (!ferror (fp));
  TEST_VERIFY (feof (fp));
  fp = FREOPEN (file2, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_VERIFY (!ferror (fp));
  TEST_VERIFY (!feof (fp));
  xfclose (fp);
  END_TEST;

  /* Test freopen with NULL, same mode (should flush content and reset
     file offset).  */

  START_TEST ("testing freopen with NULL, same mode\n");
  fp = xfopen (file1, "r+");
  ret = fputs ("same mode", fp);
  TEST_VERIFY (ret >= 0);
  fp = FREOPEN (NULL, "r+", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "same mode");
  xfclose (fp);
  END_TEST;

  /* Test freopen with NULL, different mode.  */

  START_TEST ("testing freopen with NULL, different mode\n");
  fp = xfopen (file1, "w");
  ret = fputs ("different mode", fp);
  TEST_VERIFY (ret >= 0);
  fp = FREOPEN (NULL, "r", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "different mode");
  xfclose (fp);
  END_TEST;

  /* Test freopen with NULL, renamed file.  This verifies that
     reopening succeeds (and resets the file position indicator to
     start of file) even when the original path could no longer be
     opened.  */

  START_TEST ("testing freopen with NULL, renamed file\n");
  fp = xfopen (file1, "r+");
  ret = fputs ("file has been renamed", fp);
  TEST_VERIFY (ret >= 0);
  ret = rename (file1, file1a);
  TEST_COMPARE (ret, 0);
  fp = FREOPEN (NULL, "r+", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "file has been renamed");
  xfclose (fp);
  ret = rename (file1a, file1);
  TEST_COMPARE (ret, 0);
  END_TEST;

  /* Test freopen with NULL, deleted file.  This verifies that
     reopening succeeds (and resets the file position indicator to
     start of file) even when the original path could no longer be
     opened.  */

  START_TEST ("testing freopen with NULL, deleted file\n");
  fp = xfopen (file1, "r+");
  ret = fputs ("file has now been deleted", fp);
  TEST_VERIFY (ret >= 0);
  ret = remove (file1);
  TEST_COMPARE (ret, 0);
  fp = FREOPEN (NULL, "r+", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_COMPARE_FILE_STRING (fp, "file has now been deleted");
  xfclose (fp);
  /* Recreate the file so it is present when expected for temporary
     file deletion.  */
  support_write_file_string (file1, "file1");
  END_TEST;

  free (temp_dir);
  free (file1);
  free (file2);
  free (file3);
  free (file4);
  free (file1a);
  return 0;
}

#include <support/test-driver.c>
