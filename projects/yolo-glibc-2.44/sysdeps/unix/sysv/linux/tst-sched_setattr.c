/* Tests for sched_setattr and sched_getattr.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sched.h>

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <support/check.h>
#include <sys/resource.h>
#include <unistd.h>

/* Padding struct to detect unexpected writes.   */
union
{
  struct sched_attr attr;
  /* Hopefully the kernel will never need as much.  */
  unsigned char padding[4096];
} u;

static int
do_test (void)
{
  _Static_assert (SCHED_OTHER == SCHED_NORMAL,
                  "SCHED_OTHER, SCHED_NORMAL values");
  TEST_VERIFY (sizeof (struct sched_attr) < sizeof (u));

  /* Check that reading and re-applying the current policy works.  */
  memset (&u, 0xcc, sizeof (u));
  /* Compiler barrier to bypass write access attribute.  */
  volatile unsigned int size = sizeof (u);
  TEST_COMPARE (sched_getattr (0, (struct sched_attr *) &u, size, 0), 0);
  TEST_COMPARE (sched_setattr (0, &u.attr, 0), 0); /* Apply unchanged.  */

  /* Try to switch to the SCHED_OTHER policy.   */
  memset (&u, 0, sizeof (u));
  u.attr.size = sizeof (u); /* With padding, kernel should accept zeroes.  */
  u.attr.sched_policy = SCHED_OTHER; /* Should be the default.  */
  {
    errno = 0;
    int prio = getpriority (PRIO_PROCESS, 0);
    if (errno != 0)
      prio = 0;
    u.attr.sched_nice = prio;
  }
  TEST_COMPARE (sched_setattr (0, &u.attr, 0), 0);

  /* Non-zero values not known to the kernel result in an E2BIG error.  */
  memset (&u, 0, sizeof (u));
  TEST_COMPARE (sched_getattr (0, (struct sched_attr *) &u, size, 0), 0);
  u.padding[u.attr.size] = 0xcc;
  u.attr.size = sizeof (u);
  errno = 0;
  TEST_COMPARE (sched_setattr (0, &u.attr, 0), -1);
  TEST_COMPARE (errno, E2BIG);

  memset (&u, 0xcc, sizeof (u));
  TEST_COMPARE (sched_getattr (0, (struct sched_attr *) &u, size, 0), 0);
  TEST_COMPARE (u.attr.sched_policy, SCHED_OTHER);

  /* Raise the niceless level to 19 and observe its effect.  */
  TEST_COMPARE (nice (19), 19);
  TEST_COMPARE (sched_getattr (0, &u.attr, sizeof (u.attr), 0), 0);
  TEST_COMPARE (u.attr.sched_policy, SCHED_OTHER);
  TEST_COMPARE (u.attr.sched_nice, 19);

  /* Invalid buffer arguments result in EINVAL (not EFAULT).  */
  {
    errno = 0;
    void *volatile null_pointer = NULL; /* compiler barrier.  */
    TEST_COMPARE (sched_setattr (0, null_pointer, 0), -1);
    TEST_COMPARE (errno, EINVAL);
    errno = 0;
    TEST_COMPARE (sched_getattr (0, null_pointer, size, 0), -1);
    TEST_COMPARE (errno, EINVAL);
  }

  return 0;
}

#include <support/test-driver.c>
