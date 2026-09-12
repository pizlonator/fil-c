/* Test offset of input file descriptor after close (bug 12724).
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

#include <errno.h>
#include <stdio.h>
#include <wchar.h>

#include <support/check.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

int
do_test (void)
{
  char *filename = NULL;
  int fd = create_temp_file ("tst-fclose-offset", &filename);
  TEST_VERIFY_EXIT (fd != -1);

  /* Test offset of open file description for output and input streams
     after fclose, case from bug 12724.  */

  const char buf[] = "hello world";
  xwrite (fd, buf, sizeof buf);
  TEST_COMPARE (lseek (fd, 1, SEEK_SET), 1);
  int fd2 = xdup (fd);
  FILE *f = fdopen (fd2, "w");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (fputc (buf[1], f), buf[1]);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 2);

  /* Likewise for an input stream.  */
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (fgetc (f), buf[2]);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 3);

  /* Test offset of open file description for output and input streams
     after fclose, case from comment on bug 12724 (failed after first
     attempt at fixing that bug).  This verifies that the offset is
     not reset when there has been no input or output on the FILE* (in
     that case, the FILE* might not be the active handle).  */

  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  xwrite (fd, buf, sizeof buf);
  TEST_COMPARE (lseek (fd, 1, SEEK_SET), 1);
  fd2 = xdup (fd);
  f = fdopen (fd2, "w");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (lseek (fd, 4, SEEK_SET), 4);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 4);

  /* Likewise for an input stream.  */
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (lseek (fd, 4, SEEK_SET), 4);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 4);

  /* Further cases without specific tests in bug 12724, to verify
     proper operation of the rules about the offset only being set
     when the stream is the active handle.  */

  /* Test offset set by fclose after fseek and fgetc.  */
  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (fseek (f, 1, SEEK_SET), 0);
  TEST_COMPARE (fgetc (f), buf[1]);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 2);

  /* Test offset not set by fclose after fseek and fgetc, if that
     fgetc is at EOF (in which case the active handle might have
     changed).  */
  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (fseek (f, sizeof buf, SEEK_SET), 0);
  TEST_COMPARE (fgetc (f), EOF);
  TEST_COMPARE (lseek (fd, 4, SEEK_SET), 4);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 4);

  /* Test offset not set by fclose after fseek and fgetc and fflush
     (active handle might have changed after fflush).  */
  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (fseek (f, 1, SEEK_SET), 0);
  TEST_COMPARE (fgetc (f), buf[1]);
  TEST_COMPARE (fflush (f), 0);
  TEST_COMPARE (lseek (fd, 4, SEEK_SET), 4);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 4);

  /* Test offset not set by fclose after fseek and fgetc, if the
     stream is unbuffered (active handle might change at any
     time).  */
  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  setbuf (f, NULL);
  TEST_COMPARE (fseek (f, 1, SEEK_SET), 0);
  TEST_COMPARE (fgetc (f), buf[1]);
  TEST_COMPARE (lseek (fd, 4, SEEK_SET), 4);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 4);

  /* Also test such cases with the stream in wide mode.  */

  /* Test offset set by fclose after fseek and fgetwc.  */
  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (fseek (f, 1, SEEK_SET), 0);
  TEST_COMPARE (fgetwc (f), (wint_t) buf[1]);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 2);

  /* Test offset not set by fclose after fseek and fgetwc, if that
     fgetwc is at EOF (in which case the active handle might have
     changed).  */
  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (fseek (f, sizeof buf, SEEK_SET), 0);
  TEST_COMPARE (fgetwc (f), WEOF);
  TEST_COMPARE (lseek (fd, 4, SEEK_SET), 4);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 4);

  /* Test offset not set by fclose after fseek and fgetwc and fflush
     (active handle might have changed after fflush).  */
  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  TEST_COMPARE (fseek (f, 1, SEEK_SET), 0);
  TEST_COMPARE (fgetwc (f), (wint_t) buf[1]);
  TEST_COMPARE (fflush (f), 0);
  TEST_COMPARE (lseek (fd, 4, SEEK_SET), 4);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 4);

  /* Test offset not set by fclose after fseek and fgetwc, if the
     stream is unbuffered (active handle might change at any
     time).  */
  TEST_COMPARE (lseek (fd, 0, SEEK_SET), 0);
  fd2 = xdup (fd);
  f = fdopen (fd2, "r");
  TEST_VERIFY_EXIT (f != NULL);
  setbuf (f, NULL);
  TEST_COMPARE (fseek (f, 1, SEEK_SET), 0);
  TEST_COMPARE (fgetwc (f), (wint_t) buf[1]);
  TEST_COMPARE (lseek (fd, 4, SEEK_SET), 4);
  xfclose (f);
  errno = 0;
  TEST_COMPARE (lseek (fd2, 0, SEEK_CUR), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (lseek (fd, 0, SEEK_CUR), 4);

  return 0;
}

#include <support/test-driver.c>
