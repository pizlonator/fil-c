/* Monotonically increasing wide counters (at least 62 bits).
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

#ifndef _ATOMIC_WIDE_COUNTER_H
#define _ATOMIC_WIDE_COUNTER_H

#include <atomic.h>
#include <bits/atomic_wide_counter.h>

#if __HAVE_64B_ATOMICS

static inline uint64_t
__atomic_wide_counter_load_relaxed (__atomic_wide_counter *c)
{
  return atomic_load_relaxed (&c->__value64);
}

static inline uint64_t
__atomic_wide_counter_load_acquire (__atomic_wide_counter *c)
{
  return atomic_load_acquire (&c->__value64);
}

static inline uint64_t
__atomic_wide_counter_fetch_add_relaxed (__atomic_wide_counter *c,
                                         unsigned int val)
{
  return atomic_fetch_add_relaxed (&c->__value64, val);
}

static inline uint64_t
__atomic_wide_counter_fetch_add_acquire (__atomic_wide_counter *c,
                                         unsigned int val)
{
  return atomic_fetch_add_acquire (&c->__value64, val);
}

static inline void
__atomic_wide_counter_add_relaxed (__atomic_wide_counter *c,
                                   unsigned int val)
{
  atomic_store_relaxed (&c->__value64,
                        atomic_load_relaxed (&c->__value64) + val);
}

static uint64_t __attribute__ ((unused))
__atomic_wide_counter_fetch_xor_release (__atomic_wide_counter *c,
                                         unsigned int val)
{
  return atomic_fetch_xor_release (&c->__value64, val);
}

#else /* !__HAVE_64B_ATOMICS */

uint64_t __atomic_wide_counter_load_relaxed (__atomic_wide_counter *c)
  attribute_hidden;

static inline uint64_t
__atomic_wide_counter_load_acquire (__atomic_wide_counter *c)
{
  uint64_t r = __atomic_wide_counter_load_relaxed (c);
  atomic_thread_fence_acquire ();
  return r;
}

uint64_t __atomic_wide_counter_fetch_add_relaxed (__atomic_wide_counter *c,
                                                  unsigned int op)
  attribute_hidden;

static inline uint64_t
__atomic_wide_counter_fetch_add_acquire (__atomic_wide_counter *c,
                                         unsigned int val)
{
  uint64_t r = __atomic_wide_counter_fetch_add_relaxed (c, val);
  atomic_thread_fence_acquire ();
  return r;
}

static inline void
__atomic_wide_counter_add_relaxed (__atomic_wide_counter *c,
                                   unsigned int val)
{
  __atomic_wide_counter_fetch_add_relaxed (c, val);
}

#endif /* !__HAVE_64B_ATOMICS */

#endif /* _ATOMIC_WIDE_COUNTER_H */
