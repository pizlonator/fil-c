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

#define LOCK(lock) pthread_spin_lock (lock)
#define UNLOCK(lock) pthread_spin_unlock (lock)
#define LOCK_INIT(lock, attr) pthread_spin_init (lock, *(attr))
#define LOCK_DESTROY(lock) pthread_spin_destroy (lock)
#define LOCK_ATTR_INIT(attr) *(attr) = 0

#define bench_lock_t pthread_spinlock_t
#define bench_lock_attr_t int

#define TEST_NAME "pthread-spin-lock"

#include "bench-pthread-lock-base.c"
