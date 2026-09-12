/* Add a cancelation handler to the stack.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library;  if not, see
   <https://www.gnu.org/licenses/>.  */

#include <pthread.h>

#include <pt-internal.h>
#include <shlib-compat.h>

static __thread struct __pthread_cancelation_handler *__pthread_cleanup_stack;

struct __pthread_cancelation_handler **
___pthread_get_cleanup_stack (void)
{
  return &__pthread_cleanup_stack;
}
versioned_symbol (libc, ___pthread_get_cleanup_stack, __pthread_get_cleanup_stack, GLIBC_2_21);
libc_hidden_ver (___pthread_get_cleanup_stack, __pthread_get_cleanup_stack)

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_21)
compat_symbol (libpthread, ___pthread_get_cleanup_stack, __pthread_get_cleanup_stack, GLIBC_2_12);
#endif
