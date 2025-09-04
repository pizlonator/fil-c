/* BZ #19408 linux personality syscall wrapper test.

   Copyright (C) 2015-2024 Free Software Foundation, Inc.
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
#include <sys/personality.h>
#include <support/check.h>

static int
do_test (void)
{
  unsigned int test_persona = -EINVAL;
  unsigned int saved_persona;

  errno = 0xdefaced;
  saved_persona = personality (0xffffffff);

  unsigned int r = personality (test_persona);
  if (r == -1)
    {
      /* The syscall argument might be filtered by kernel, so the
        test can not check for the bug issue.  */
      if (errno == EPERM)
       FAIL_UNSUPPORTED ("personality syscall argument are filtered");
      FAIL_EXIT1 ("personality (%#x) failed: %m", test_persona);
    }

  TEST_COMPARE (r, saved_persona);
  TEST_VERIFY (personality (0xffffffff) != -1);
  TEST_VERIFY (personality (PER_LINUX) != -1);
  TEST_COMPARE (personality (0xffffffff), PER_LINUX);
  TEST_COMPARE (0xdefaced, errno);

  personality (saved_persona);

  return 0;
}

#include <support/test-driver.c>
