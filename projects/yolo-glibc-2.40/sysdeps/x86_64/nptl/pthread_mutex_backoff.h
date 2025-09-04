/* Pthread mutex backoff configuration.
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
#ifndef _PTHREAD_MUTEX_BACKOFF_H
#define _PTHREAD_MUTEX_BACKOFF_H 1

#include <fast-jitter.h>

static inline unsigned int
get_jitter (void)
{
  return get_fast_jitter ();
}

#define MAX_BACKOFF 16

static inline int
get_next_backoff (int backoff)
{
  /* Binary expontial backoff. Limiting max backoff
     can reduce latency in large critical section.  */
  return (backoff < MAX_BACKOFF) ? backoff << 1 : backoff;
}

#endif
