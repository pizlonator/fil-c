/* Test the fdopen function.
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

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xunistd.h>
#include <support/temp_file.h>

char *tmp_dir;
char *path_to_file;

void
prepare_tmp_dir (void)
{
  tmp_dir = support_create_temp_directory ("tst-fdopen2");
  path_to_file = xasprintf ("%s/tst-fdopen2.txt", tmp_dir);
}

/* open temp file descriptor with mode.  */
int
open_tmp_fd (int mode)
{
  int fd = xopen (path_to_file, mode, 0644);
  return fd;
}


/* close and remove temp file with close.  */
void
close_tmp_fd (int fd)
{
  xclose (fd);
  xunlink (path_to_file);
}

/* close and remove temp file with fclose.  */
void
close_tmp_fp (FILE *fp)
{
  fclose (fp);
  xunlink (path_to_file);
}

/* test "w" fdopen mode.  */
void
do_test_fdopen_w (void)
{
  int fd, ret;
  FILE *fp;
  fd = open_tmp_fd (O_WRONLY | O_CREAT | O_TRUNC);

  /* test mode mismatch.  */
  fp = fdopen (fd, "r");
  if (fp != NULL || errno != EINVAL)
    {
      close_tmp_fd (fd);
      FAIL_EXIT1 ("fdopen (%d, r) should fail with EINVAL: %m", fd);
    }

  fp = fdopen (fd, "w");
  if (fp == NULL)
    {
      close_tmp_fd (fd);
      FAIL_EXIT1 ("fdopen (%d, w): %m", fd);
    }

  const void *buf = "AAAA";
  ret = fwrite (buf, 1, 4, fp);
  if (ret != 4)
    {
      close_tmp_fp (fp);
      FAIL_EXIT1 ("fwrite (): %m");
    }

  unsigned char buf2[4];
  rewind (fp);
  clearerr (fp);
  /* fread should fail in "w" mode  */
  ret = fread (buf2, 1, 4, fp);
  if (ret != 0 || ferror (fp) == 0)
    {
      close_tmp_fp (fp);
      FAIL_EXIT1 ("fread should fail in \"w\" mode");
    }

  fclose (fp);
}

/* test "r" fdopen mode. */
void
do_test_fdopen_r (void)
{
  int fd, ret;
  FILE *fp;
  fd = open_tmp_fd (O_RDONLY);

  /* test mode mismatch. */
  fp = fdopen (fd, "w");
  if (fp != NULL || errno != EINVAL)
    {
      close_tmp_fd (fd);
      FAIL_EXIT1 ("fdopen (%d, w) should fail with EINVAL: %m", fd);
    }

  fp = fdopen (fd, "r");
  if (fp == NULL)
    {
      close_tmp_fd (fd);
      FAIL_EXIT1 ("fdopen (%d, w): %m", fd);
    }

  const void *buf = "BBBB";
  /* fwrite should fail in "r" mode.  */
  ret = fwrite (buf, 1, 4, fp);
  if (ret != 0 || ferror (fp) == 0)
    {
      close_tmp_fp (fp);
      FAIL_EXIT1 ("fwrite should fail in \"r\" mode");
    }

  unsigned char buf2[4];
  ret = fread (buf2, 1, 4, fp);
  if (ret != 4)
    {
      close_tmp_fp (fp);
      FAIL_EXIT1 ("fread (): %m");
    }

  fclose (fp);
}

/* test "a" fdopen mode.  */
void
do_test_fdopen_a (void)
{
  int fd, ret;
  FILE *fp;
  fd = open_tmp_fd (O_WRONLY | O_CREAT | O_APPEND);

  /* test mode mismatch.  */
  fp = fdopen (fd, "r+");
  if (fp != NULL || errno != EINVAL)
    {
      close_tmp_fd (fd);
      FAIL_EXIT1 ("fdopen (%d, \"r+\") should fail with EINVAL: %m", fd);
    }

  fp = fdopen (fd, "a");
  if (fp == NULL)
    {
      close_tmp_fd (fd);
      FAIL_EXIT1 ("fdopen (%d, w): %m", fd);
    }

  const void *buf = "CCCC";
  ret = fwrite (buf, 1, 4, fp);
  if (ret != 4)
    {
      close_tmp_fp (fp);
      FAIL_EXIT1 ("fwrite (): %m");
    }

  /* fread should fail in "a" mode.  */
  unsigned char buf2[4];
  clearerr (fp);
  ret = fread (buf2, 1, 4, fp);
  if (ret != 0 || ferror (fp) == 0)
    {
      close_tmp_fp (fp);
      FAIL_EXIT1 ("fread should fail \"a\" mode");
    }

  fclose (fp);
}

void
do_test_fdopen_mode (int mode, const char *fmode)
{
  int fd, ret;
  FILE *fp;
  fd = open_tmp_fd (mode);

  fp = fdopen (fd, fmode);
  if (fp == NULL)
    {
      close_tmp_fd (fd);
      FAIL_EXIT1 ("fdopen (%d, %s): %m", fd, fmode);
    }

  const void *buf = "EEEE";
  ret = fwrite (buf, 1, 4, fp);
  if (ret != 4)
    {
      close_tmp_fp (fp);
      FAIL_EXIT1 ("fwrite () in mode:%s returns %d: %m", fmode, ret);
    }

  rewind (fp);
  unsigned char buf2[4];
  ret = fread (buf2, 1, 4, fp);
  if (ret != 4)
    {
      close_tmp_fp (fp);
      FAIL_EXIT1 ("fread () in mode:%s returns %d: %m", fmode, ret);
    }

  fclose (fp);
}

static int
do_test (void)
{

  prepare_tmp_dir ();

  do_test_fdopen_w ();
  do_test_fdopen_r ();
  do_test_fdopen_a ();

  /* test r+ w+ a+ fdopen modes.  */
  do_test_fdopen_mode (O_RDWR, "r+");
  do_test_fdopen_mode (O_RDWR | O_CREAT | O_TRUNC, "w+");
  do_test_fdopen_mode (O_RDWR | O_CREAT | O_APPEND, "a+");
  xunlink (path_to_file);
  return 0;
}

#include <support/test-driver.c>
