/* CPU Affinity inheritance test - pthread_{gs}etaffinity_np.
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
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>

static void
get_my_affinity (size_t size, cpu_set_t *set)
{
  int ret = pthread_getaffinity_np (pthread_self (), size, set);
  if (ret != 0)
    FAIL ("pthread_getaffinity_np returned %d (%s)", ret, strerror (ret));
}

static void
set_my_affinity (size_t size, const cpu_set_t *set)
{
  int ret = pthread_setaffinity_np (pthread_self (), size, set);
  if (ret != 0)
    FAIL ("pthread_setaffinity_np returned %d (%s)", ret, strerror (ret));
}

#include "tst-skeleton-affinity-inheritance.c"
