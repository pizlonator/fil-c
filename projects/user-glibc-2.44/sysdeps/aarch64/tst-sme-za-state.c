/* Test for SME ZA state being cleared on setjmp and longjmp.
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

#include <setjmp.h>

static void
run (struct blk *ptr)
{
  jmp_buf buf;
  int ret;

  check_sme_za_state ("initial state", /* Clear.  */ true);

  /* Enabled ZA state so that effect of disabling be observable.  */
  enable_sme_za_state (ptr);
  check_sme_za_state ("before setjmp", /* Clear.  */ false);

  if ((ret = setjmp (buf)) == 0)
    {
      check_sme_za_state ("after setjmp", /* Clear.  */ true);

      /* Enabled ZA state so that effect of disabling be observable.  */
      enable_sme_za_state (ptr);
      check_sme_za_state ("before longjmp", /* Clear.  */ false);

      longjmp (buf, 42);

      /* Unreachable.  */
      TEST_VERIFY (false);
      __builtin_unreachable ();
    }

  TEST_COMPARE (ret, 42);
  check_sme_za_state ("after longjmp", /* Clear.  */ true);
}
