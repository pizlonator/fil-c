/* Tests for sched_getaffinity with large buffers.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <sched.h>
#include <support/check.h>

/* NB: this test may fail on system with more than 32k cpus.  */

static int
do_test (void)
{
  /* The values are larger than the default cpu_set_t.  */
  const int bufsize[] = { 1<<11, 1<<12, 1<<13, 1<<14, 1<<15, 1<<16, 1<<17 };
  int cpucount[array_length (bufsize)];

  for (int i = 0; i < array_length (bufsize); i++)
    {
      cpu_set_t *cpuset = CPU_ALLOC (bufsize[i]);
      TEST_VERIFY (cpuset != NULL);
      size_t size = CPU_ALLOC_SIZE (bufsize[i]);
      TEST_COMPARE (sched_getaffinity (0, size, cpuset), 0);
      cpucount[i] = CPU_COUNT_S (size, cpuset);
      CPU_FREE (cpuset);
    }

  for (int i = 0; i < array_length (cpucount) - 1; i++)
    TEST_COMPARE (cpucount[i], cpucount[i + 1]);

  return 0;
}

#include <support/test-driver.c>
