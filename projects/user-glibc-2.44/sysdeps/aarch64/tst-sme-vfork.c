/* Test that ZA state of SME is cleared in both parent and child
   when vfork() function is used.
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

#include "tst-sme-skeleton.c"

static void
run (struct blk *blk)
{
  /* Enabled ZA state so that effect of disabling be observable.  */
  enable_sme_za_state (blk);
  check_sme_za_state ("before vfork", /* Clear.  */ false);
  fflush (stdout);

  pid_t pid = vfork ();

  if (pid == 0)
    {
      /* Check that ZA state of SME was disabled in child.  */
      check_sme_za_state ("after vfork in child", /* Clear.  */ true);
      _exit (0);
    }

  /* Check that ZA state of SME was disabled in parent.  */
  check_sme_za_state ("after vfork in parent", /* Clear.  */ true);

  TEST_VERIFY (xwaitpid (pid, NULL, 0) == pid);
}
