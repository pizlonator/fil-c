/* Measure mutex_trylock for different threads and critical sections.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#define LOCK(lock)                                                            \
  while (pthread_mutex_trylock (lock) != 0)                                   \
    {                                                                         \
      non_critical_section (non_crt_len);                                     \
    }

#define UNLOCK(lock) pthread_mutex_unlock (lock)
#define LOCK_INIT(lock, attr) pthread_mutex_init (lock, attr)
#define LOCK_DESTROY(lock) pthread_mutex_destroy (lock)
#define LOCK_ATTR_INIT(attr)                                                  \
  pthread_mutexattr_init (attr);                                              \
  pthread_mutexattr_settype (attr, PTHREAD_MUTEX_ADAPTIVE_NP);

#define bench_lock_t pthread_mutex_t
#define bench_lock_attr_t pthread_mutexattr_t

#define TEST_NAME "pthread-mutex-trylock"

#include "bench-pthread-lock-base.c"
