/* CPU Affinity inheritance test - sched_{gs}etaffinity.
   Copyright The GNU Toolchain Authors.
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

/* See top level comment in nptl/tst-skeleton-affinity-inheritance.c for a
   description of this test.  */

#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <support/check.h>

static void
get_my_affinity (size_t size, cpu_set_t *set)
{
  int ret = sched_getaffinity (0, size, set);
  if (ret != 0)
    FAIL ("sched_getaffinity returned %d (%s)", ret, strerror (ret));
}

static void
set_my_affinity (size_t size, const cpu_set_t *set)
{
  int ret = sched_setaffinity (0, size, set);

  if (ret != 0)
    FAIL ("sched_setaffinity returned %d (%s)", ret, strerror (ret));
}

#include <nptl/tst-skeleton-affinity-inheritance.c>
