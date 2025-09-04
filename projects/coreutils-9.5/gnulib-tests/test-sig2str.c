/* Test the sig2str and str2sig functions.
   Copyright (C) 2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Collin Funk <collin.funk1@gmail.com>, 2024.  */

#include <config.h>

/* Specification.  */
#include "sig2str.h"

#include <string.h>

#include "macros.h"

static void
test_sig2str (void)
{
  char buffer[SIG2STR_MAX];

  /* Test sig2str on signals specified by ISO C.  */

  ASSERT (sig2str (SIGABRT, buffer) == 0);
  ASSERT (STREQ (buffer, "ABRT"));

  ASSERT (sig2str (SIGFPE, buffer) == 0);
  ASSERT (STREQ (buffer, "FPE"));

  ASSERT (sig2str (SIGILL, buffer) == 0);
  ASSERT (STREQ (buffer, "ILL"));

  ASSERT (sig2str (SIGINT, buffer) == 0);
  ASSERT (STREQ (buffer, "INT"));

  ASSERT (sig2str (SIGSEGV, buffer) == 0);
  ASSERT (STREQ (buffer, "SEGV"));

  ASSERT (sig2str (SIGTERM, buffer) == 0);
  ASSERT (STREQ (buffer, "TERM"));

  /* Check behavior of sig2str on invalid signals.  */

  ASSERT (sig2str (-714, buffer) == -1);
}

static void
test_str2sig (void)
{
  int signo;

  /* Test str2sig on signals specified by ISO C.  */

  ASSERT (str2sig ("ABRT", &signo) == 0);
  ASSERT (signo == SIGABRT);

  ASSERT (str2sig ("FPE", &signo) == 0);
  ASSERT (signo == SIGFPE);

  ASSERT (str2sig ("ILL", &signo) == 0);
  ASSERT (signo == SIGILL);

  ASSERT (str2sig ("INT", &signo) == 0);
  ASSERT (signo == SIGINT);

  ASSERT (str2sig ("SEGV", &signo) == 0);
  ASSERT (signo == SIGSEGV);

  ASSERT (str2sig ("TERM", &signo) == 0);
  ASSERT (signo == SIGTERM);

  /* Check behavior of str2sig on invalid signals.  */

  ASSERT (str2sig ("Not a signal", &signo) == -1);
}

int
main (void)
{
  test_sig2str ();
  test_str2sig ();

  return 0;
}
