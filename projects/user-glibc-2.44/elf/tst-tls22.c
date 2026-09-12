/* Test TLS with varied alignment and multiple modules and threads.
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

#include <support/check.h>
#include <support/xthread.h>
#include <tst-tls22.h>

static void
check_addrs_align (const struct obj_addrs *addrs)
{
  TEST_COMPARE (addrs->addr_tdata1 & (__alignof__ (int) - 1), 0);
  TEST_COMPARE (addrs->addr_tdata2 & 0xf, 0);
  TEST_COMPARE (addrs->addr_tdata3 & 0xfff, 0);
  TEST_COMPARE (addrs->addr_tbss1 & (__alignof__ (int) - 1), 0);
  TEST_COMPARE (addrs->addr_tbss2 & 0xf, 0);
  TEST_COMPARE (addrs->addr_tbss3 & 0xfff, 0);
}

static void
check_addrs_same (const struct obj_addrs *addrs1,
		  const struct obj_addrs *addrs2)
{
  TEST_COMPARE (addrs1->addr_tdata1, addrs2->addr_tdata1);
  TEST_COMPARE (addrs1->addr_tdata2, addrs2->addr_tdata2);
  TEST_COMPARE (addrs1->addr_tdata3, addrs2->addr_tdata3);
  TEST_COMPARE (addrs1->addr_tbss1, addrs2->addr_tbss1);
  TEST_COMPARE (addrs1->addr_tbss2, addrs2->addr_tbss2);
  TEST_COMPARE (addrs1->addr_tbss3, addrs2->addr_tbss3);
}

static void
check_vals_before (const struct obj_values *vals)
{
  TEST_COMPARE (vals->val_tdata1, 1);
  TEST_COMPARE (vals->val_tdata2, 2);
  TEST_COMPARE (vals->val_tdata3, 4);
  TEST_COMPARE (vals->val_tbss1, 0);
  TEST_COMPARE (vals->val_tbss2, 0);
  TEST_COMPARE (vals->val_tbss3, 0);
}

static void
check_vals_after (const struct obj_values *vals, int base_val)
{
  TEST_COMPARE (vals->val_tdata1, base_val);
  TEST_COMPARE (vals->val_tdata2, base_val + 1);
  TEST_COMPARE (vals->val_tdata3, base_val + 2);
  TEST_COMPARE (vals->val_tbss1, base_val + 3);
  TEST_COMPARE (vals->val_tbss2, base_val + 4);
  TEST_COMPARE (vals->val_tbss3, base_val + 5);
}

static void
check_one_thread (const struct one_thread_data *data, int base_val)
{
  check_vals_before (&data->exe_before);
  check_vals_before (&data->mod1_before);
  check_vals_before (&data->mod2_before);
  check_vals_after (&data->exe_after, base_val);
  check_vals_after (&data->mod1_after, base_val);
  check_vals_after (&data->mod2_after, base_val);
  check_addrs_align (&data->exe_self);
  check_addrs_same (&data->exe_self, &data->exe_from_mod1);
  check_addrs_same (&data->exe_self, &data->exe_from_mod2);
  check_addrs_align (&data->mod1_self);
  check_addrs_same (&data->mod1_self, &data->mod1_from_exe);
  check_addrs_align (&data->mod2_self);
  check_addrs_same (&data->mod2_self, &data->mod2_from_exe);
  check_addrs_same (&data->mod2_self, &data->mod2_from_mod1);
}

static void *
thread_func (void *arg)
{
  int base_val = (int) (intptr_t) arg + 10;
  struct one_thread_data data;
  /* Record the addresses of variables as seen from the main
     executable (which should be the same as seen from the other
     modules), and their initial values.  */
  STORE_ADDRS (&data.exe_self, exe);
  STORE_ADDRS (&data.mod1_from_exe, mod1);
  STORE_ADDRS (&data.mod2_from_exe, mod2);
  STORE_VALUES (&data.exe_before, exe);
  STORE_VALUES (&data.mod1_before, mod1);
  STORE_VALUES (&data.mod2_before, mod2);
  /* Overwrite the value of variables.  */
  OVERWRITE_VALUES (exe, base_val);
  OVERWRITE_VALUES (mod1, base_val);
  OVERWRITE_VALUES (mod2, base_val);
  /* Record the addresses of variables as seen from other modules.  */
  test_mod1 (&data, base_val);
  test_mod2 (&data, base_val);
  /* Record the overwritten values (thus making sure that no other
     thread running in parallel has changed this thread's values).  */
  STORE_VALUES (&data.exe_after, exe);
  STORE_VALUES (&data.mod1_after, mod1);
  STORE_VALUES (&data.mod2_after, mod2);
  /* Check all the addresses and values recorded.  */
  check_one_thread (&data, base_val);
  return NULL;
}

#define NUM_ITERS 50
#define NUM_THREADS 16

/* For NUM_ITERS iterations, repeatedly create NUM_THREADS threads.
   In each thread, we determine the addresses of TLS objects (both
   from the module defining those objects and from other modules), and
   their initial values, and store in values that are then read back;
   we check that each object's address is the same regardless of the
   module in which it is determined, that alignment of objects is as
   required, and that the values of objects are as expected.  */

static int
do_test (void)
{
  for (size_t i = 0; i < NUM_ITERS; i++)
    {
      pthread_t threads[NUM_THREADS];
      for (size_t j = 0; j < NUM_THREADS; j++)
	threads[j] = xpthread_create (NULL, thread_func, (void *) j);
      /* Also run checks in the main thread, but only once because
	 those values don't get reinitialized.  */
      if (i == 0)
	thread_func ((void *) NUM_THREADS);
      for (size_t j = 0; j < NUM_THREADS; j++)
	xpthread_join (threads[j]);
    }
  return 0;
}

#include <support/test-driver.c>
