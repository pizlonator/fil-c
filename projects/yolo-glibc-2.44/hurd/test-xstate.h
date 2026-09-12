/* Helpers to test XSTATE during signal handling

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#ifndef _TEST_XSTATE_H
#define _TEST_XSTATE_H

#if defined __x86_64__ || defined __i386__
#define XSTATE_HELPERS_SUPPORTED 1
#define MMXSTATE_BUFFER_SIZE 8
#define XSTATE_BUFFER_SIZE 16
#define SET_MMXSTATE(b) do {                                  \
    asm volatile ("movq (%0),%%mm0" :: "r" (b));              \
  } while (0)
#define SET_XSTATE(b) do {                                    \
    asm volatile ("movups (%0),%%xmm0" :: "r" (b));           \
  } while (0)

#define GET_MMXSTATE(b) do {                                  \
    asm volatile ("movq %%mm0,(%0)" :: "r" (b));              \
  } while (0)
#define GET_XSTATE(b) do {                                    \
    asm volatile ("movups %%xmm0,(%0)" :: "r" (b));           \
  } while (0)

#else
#define XSTATE_HELPERS_SUPPORTED 0
#define MMXSTATE_BUFFER_SIZE 1
#define XSTATE_BUFFER_SIZE 1
#define SET_XSTATE(b)
#endif

#endif /* _TEST_XSTATE_H */
