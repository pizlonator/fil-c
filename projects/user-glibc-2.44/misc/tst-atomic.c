/* Tests for atomic.h macros.
   Copyright (C) 2003-2026 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <atomic.h>

#ifndef atomic_t
# define atomic_t int
#endif

/* Test various atomic.h macros.  */
static int
do_test (void)
{
  atomic_t mem, expected;
  int ret = 0;

#ifdef atomic_compare_and_exchange_val_acq
  mem = 24;
  if (atomic_compare_and_exchange_val_acq (&mem, 35, 24) != 24
      || mem != 35)
    {
      puts ("atomic_compare_and_exchange_val_acq test 1 failed");
      ret = 1;
    }

  mem = 12;
  if (atomic_compare_and_exchange_val_acq (&mem, 10, 15) != 12
      || mem != 12)
    {
      puts ("atomic_compare_and_exchange_val_acq test 2 failed");
      ret = 1;
    }

  mem = -15;
  if (atomic_compare_and_exchange_val_acq (&mem, -56, -15) != -15
      || mem != -56)
    {
      puts ("atomic_compare_and_exchange_val_acq test 3 failed");
      ret = 1;
    }

  mem = -1;
  if (atomic_compare_and_exchange_val_acq (&mem, 17, 0) != -1
      || mem != -1)
    {
      puts ("atomic_compare_and_exchange_val_acq test 4 failed");
      ret = 1;
    }
#endif

  mem = 24;
  if (atomic_compare_and_exchange_bool_acq (&mem, 35, 24)
      || mem != 35)
    {
      puts ("atomic_compare_and_exchange_bool_acq test 1 failed");
      ret = 1;
    }

  mem = 12;
  if (! atomic_compare_and_exchange_bool_acq (&mem, 10, 15)
      || mem != 12)
    {
      puts ("atomic_compare_and_exchange_bool_acq test 2 failed");
      ret = 1;
    }

  mem = -15;
  if (atomic_compare_and_exchange_bool_acq (&mem, -56, -15)
      || mem != -56)
    {
      puts ("atomic_compare_and_exchange_bool_acq test 3 failed");
      ret = 1;
    }

  mem = -1;
  if (! atomic_compare_and_exchange_bool_acq (&mem, 17, 0)
      || mem != -1)
    {
      puts ("atomic_compare_and_exchange_bool_acq test 4 failed");
      ret = 1;
    }

  mem = 1;
  if (atomic_decrement_if_positive (&mem) != 1
      || mem != 0)
    {
      puts ("atomic_decrement_if_positive test 1 failed");
      ret = 1;
    }

  mem = 0;
  if (atomic_decrement_if_positive (&mem) != 0
      || mem != 0)
    {
      puts ("atomic_decrement_if_positive test 2 failed");
      ret = 1;
    }

  mem = -1;
  if (atomic_decrement_if_positive (&mem) != -1
      || mem != -1)
    {
      puts ("atomic_decrement_if_positive test 3 failed");
      ret = 1;
    }

  /* Tests for C11-like atomics.  */
  mem = 11;
  if (atomic_load_relaxed (&mem) != 11 || atomic_load_acquire (&mem) != 11)
    {
      puts ("atomic_load_{relaxed,acquire} test failed");
      ret = 1;
    }

  atomic_store_relaxed (&mem, 12);
  if (mem != 12)
    {
      puts ("atomic_store_relaxed test failed");
      ret = 1;
    }
  atomic_store_release (&mem, 13);
  if (mem != 13)
    {
      puts ("atomic_store_release test failed");
      ret = 1;
    }

  mem = 14;
  expected = 14;
  if (!atomic_compare_exchange_weak_relaxed (&mem, &expected, 25)
      || mem != 25 || expected != 14)
    {
      puts ("atomic_compare_exchange_weak_relaxed test 1 failed");
      ret = 1;
    }
  if (atomic_compare_exchange_weak_relaxed (&mem, &expected, 14)
      || mem != 25 || expected != 25)
    {
      puts ("atomic_compare_exchange_weak_relaxed test 2 failed");
      ret = 1;
    }
  mem = 14;
  expected = 14;
  if (!atomic_compare_exchange_weak_acquire (&mem, &expected, 25)
      || mem != 25 || expected != 14)
    {
      puts ("atomic_compare_exchange_weak_acquire test 1 failed");
      ret = 1;
    }
  if (atomic_compare_exchange_weak_acquire (&mem, &expected, 14)
      || mem != 25 || expected != 25)
    {
      puts ("atomic_compare_exchange_weak_acquire test 2 failed");
      ret = 1;
    }
  mem = 14;
  expected = 14;
  if (!atomic_compare_exchange_weak_release (&mem, &expected, 25)
      || mem != 25 || expected != 14)
    {
      puts ("atomic_compare_exchange_weak_release test 1 failed");
      ret = 1;
    }
  if (atomic_compare_exchange_weak_release (&mem, &expected, 14)
      || mem != 25 || expected != 25)
    {
      puts ("atomic_compare_exchange_weak_release test 2 failed");
      ret = 1;
    }

  mem = 23;
  if (atomic_exchange_acquire (&mem, 42) != 23 || mem != 42)
    {
      puts ("atomic_exchange_acquire test failed");
      ret = 1;
    }
  mem = 23;
  if (atomic_exchange_release (&mem, 42) != 23 || mem != 42)
    {
      puts ("atomic_exchange_release test failed");
      ret = 1;
    }

  mem = 23;
  if (atomic_fetch_add_relaxed (&mem, 1) != 23 || mem != 24)
    {
      puts ("atomic_fetch_add_relaxed test failed");
      ret = 1;
    }
  mem = 23;
  if (atomic_fetch_add_acquire (&mem, 1) != 23 || mem != 24)
    {
      puts ("atomic_fetch_add_acquire test failed");
      ret = 1;
    }
  mem = 23;
  if (atomic_fetch_add_release (&mem, 1) != 23 || mem != 24)
    {
      puts ("atomic_fetch_add_release test failed");
      ret = 1;
    }
  mem = 23;
  if (atomic_fetch_add_acq_rel (&mem, 1) != 23 || mem != 24)
    {
      puts ("atomic_fetch_add_acq_rel test failed");
      ret = 1;
    }

  mem = 3;
  if (atomic_fetch_and_acquire (&mem, 2) != 3 || mem != 2)
    {
      puts ("atomic_fetch_and_acquire test failed");
      ret = 1;
    }

  mem = 4;
  if (atomic_fetch_or_relaxed (&mem, 2) != 4 || mem != 6)
    {
      puts ("atomic_fetch_or_relaxed test failed");
      ret = 1;
    }
  mem = 4;
  if (atomic_fetch_or_acquire (&mem, 2) != 4 || mem != 6)
    {
      puts ("atomic_fetch_or_acquire test failed");
      ret = 1;
    }

  /* This is a single-threaded test, so we can't test the effects of the
     fences.  */
  atomic_thread_fence_acquire ();
  atomic_thread_fence_release ();
  atomic_thread_fence_seq_cst ();

  return ret;
}

#include <support/test-driver.c>
