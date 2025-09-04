/* Setup a Mach thread.
   Copyright (C) 1993-2024 Free Software Foundation, Inc.
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

#ifndef	_MACH_SETUP_THREAD_H

#define	_MACH_SETUP_THREAD_H	1

#include <mach.h>

/* Like mach_setup_thread (), but suitable for setting up function
   calls.  */
kern_return_t __mach_setup_thread_call (task_t task, thread_t thread,
					void *function,
					vm_address_t *stack_base,
					vm_size_t *stack_size);

#endif	/* mach/setup-thread.h */
