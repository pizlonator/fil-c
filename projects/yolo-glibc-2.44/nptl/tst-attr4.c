/* Test initial values of pthread attributes.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <sched.h>
#include <stdio.h>
#include <stdint.h>

#include <support/check.h>
#include <support/test-driver.h>
#include <support/xthread.h>


int
do_test (void)
{
  pthread_attr_t a;
  int ret;

  xpthread_attr_init (&a);

  size_t stacksize = 0;
  verbose_printf ("testing default stack size\n");
  ret = pthread_attr_getstacksize (&a, &stacksize);
  TEST_VERIFY_EXIT (ret == 0);
  TEST_VERIFY (stacksize >= PTHREAD_STACK_MIN);
  TEST_VERIFY (stacksize <= SIZE_MAX / 2);

  int policy;
  verbose_printf ("testing default scheduler parameters\n");
  ret = pthread_attr_getschedpolicy (&a, &policy);
  TEST_VERIFY_EXIT (ret == 0);
  struct sched_param param;
  ret = pthread_attr_getschedparam (&a, &param);
  TEST_VERIFY_EXIT (ret == 0);
  int min = sched_get_priority_min (policy);
  TEST_VERIFY (min != -1);
  int max = sched_get_priority_max (policy);
  TEST_VERIFY (max != -1);
  TEST_VERIFY (param.sched_priority >= min);
  TEST_VERIFY (param.sched_priority <= max);

  xpthread_attr_destroy (&a);

  return 0;
}

#include <support/test-driver.c>
