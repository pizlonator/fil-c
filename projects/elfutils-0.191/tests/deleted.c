/* Test program for opening already deleted running binaries.
   Copyright (C) 2014 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

extern void libfunc (void);

int
main (int argc __attribute__ ((unused)), char **argv __attribute__ ((unused)))
{
  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  pid_t pid = fork ();
  assert (pid != -1);
  if (pid == 0)
    {
      int err = close (0);
      assert (!err);
      err = close (1);
      assert (!err);
      err = close (2);
      assert (!err);
      /* Make sure eu-stack -p works on this process even with
	 "restricted ptrace".  */
#ifdef PR_SET_PTRACER_ANY
      prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif
      libfunc ();
      abort ();
    }
  printf ("%d\n", pid);
  return EXIT_SUCCESS;
}
