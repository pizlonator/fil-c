/* Fallback for fast jitter just return 0.
   Copyright (C) 2019-2024 Free Software Foundation, Inc.
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

#ifndef _FAST_JITTER_H
# define _FAST_JITTER_H

# include <stdint.h>
# include <hp-timing.h>

/* Baseline just return 0.  We could create jitter using a clock or
   'random_bits' but that may imply a syscall and the goal of
   'get_fast_jitter' is minimal overhead "randomness" when such
   randomness helps performance.  Adding high overhead the function
   defeats the purpose.  */
static inline uint32_t
get_fast_jitter (void)
{
# if HP_TIMING_INLINE
  hp_timing_t jitter;
  HP_TIMING_NOW (jitter);
  return (uint32_t) jitter;
# else
  return 0;
# endif
}

#endif
