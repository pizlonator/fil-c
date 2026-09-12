/* ___pthread_self variable.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <pt-sysdep.h>
#include <pt-internal.h>

/* Initial thread structure used before libpthread is initialized, so various
 * functions can already work at least basically.  */
struct __pthread __pthread_init_thread = {
	.thread = 1,
};
libc_hidden_def (__pthread_init_thread)

__thread struct __pthread *___pthread_self = &__pthread_init_thread;
libc_hidden_tls_def (___pthread_self)
