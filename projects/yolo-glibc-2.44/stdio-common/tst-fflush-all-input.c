/* Test fflush (NULL) flushes input files (bug 32369).
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>

#include <support/check.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

int
do_test (void)
{
  FILE *temp = tmpfile ();
  TEST_VERIFY_EXIT (temp != NULL);
  fprintf (temp, "abc");
  TEST_COMPARE (fflush (temp), 0);
  TEST_COMPARE (lseek (fileno (temp), 0, SEEK_SET), 0);
  TEST_COMPARE (fgetc (temp), 'a');
  TEST_COMPARE (fflush (NULL), 0);
  TEST_COMPARE (lseek (fileno (temp), 0, SEEK_CUR), 1);
  xfclose (temp);

  /* Likewise, but in wide mode.  */
  temp = tmpfile ();
  TEST_VERIFY_EXIT (temp != NULL);
  fwprintf (temp, L"abc");
  TEST_COMPARE (fflush (temp), 0);
  TEST_COMPARE (lseek (fileno (temp), 0, SEEK_SET), 0);
  TEST_COMPARE (fgetwc (temp), L'a');
  TEST_COMPARE (fflush (NULL), 0);
  TEST_COMPARE (lseek (fileno (temp), 0, SEEK_CUR), 1);
  xfclose (temp);

  /* Similar tests, but with the flush implicitly occurring on exit
     (in a forked subprocess).  */

  temp = tmpfile ();
  TEST_VERIFY_EXIT (temp != NULL);
  pid_t pid = xfork ();
  if (pid == 0)
    {
      fprintf (temp, "abc");
      TEST_COMPARE (fflush (temp), 0);
      TEST_COMPARE (lseek (fileno (temp), 0, SEEK_SET), 0);
      TEST_COMPARE (fgetc (temp), 'a');
      exit (EXIT_SUCCESS);
    }
  else
    {
      TEST_COMPARE (xwaitpid (pid, NULL, 0), pid);
      TEST_COMPARE (lseek (fileno (temp), 0, SEEK_CUR), 1);
      xfclose (temp);
    }

  temp = tmpfile ();
  TEST_VERIFY_EXIT (temp != NULL);
  pid = xfork ();
  if (pid == 0)
    {
      fwprintf (temp, L"abc");
      TEST_COMPARE (fflush (temp), 0);
      TEST_COMPARE (lseek (fileno (temp), 0, SEEK_SET), 0);
      TEST_COMPARE (fgetwc (temp), L'a');
      exit (EXIT_SUCCESS);
    }
  else
    {
      TEST_COMPARE (xwaitpid (pid, NULL, 0), pid);
      TEST_COMPARE (lseek (fileno (temp), 0, SEEK_CUR), 1);
      xfclose (temp);
    }

  return 0;
}

#include <support/test-driver.c>
