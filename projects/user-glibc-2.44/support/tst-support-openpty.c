/* Basic test for support_openpty support in test-container.
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

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <support/tty.h>
#include <support/check.h>
#include <support/support.h>

/* Note: the purpose of this test isn't to test if ptys function
   correctly, but only to verify that test-container's support for
   them is correct.  The many checks in support_openpty.c are
   sufficient for this.  */

int
do_test (void)
{
  int outer, inner;
  char *name;
  struct termios term;
  struct winsize win;

  cfmakeraw (&term);
  win.ws_row = 24;
  win.ws_col = 80;

  support_openpty (&outer, &inner, &name, &term, &win);

  return 0;
}

#include <support/test-driver.c>
