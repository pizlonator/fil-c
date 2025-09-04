/* High precision, low overhead timing functions.
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

#ifndef _HP_TIMING_H
#define _HP_TIMING_H 1

/* We always assume having the timestamp register.  */
#define HP_TIMING_AVAIL (1)
#define HP_SMALL_TIMING_AVAIL (1)

/* We indeed have inlined functions.  */
#define HP_TIMING_INLINE (1)

/* We use 64bit values for the times.  */
typedef unsigned long long int hp_timing_t;

/* Read the stable counter.  */
#define HP_TIMING_NOW(Var) \
  ({ \
    unsigned long long int _count; \
    asm volatile ("rdtime.d\t%0,$r0" : "=r" (_count)); \
    (Var) = _count; \
  })

#include <hp-timing-common.h>

#endif /* hp-timing.h */
