/* Internal per-thread variables for the Hurd.
   Copyright (C) 1994-2024 Free Software Foundation, Inc.
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

#ifndef _HURD_THREADVAR_H
#define	_HURD_THREADVAR_H

#include <features.h>
#include <tls.h>

/* The variables __hurd_sigthread_stack_base and
   __hurd_sigthread_stack_end define the bounds of the stack used by the
   signal thread, so that thread can always be specifically identified.  */

extern unsigned long int __hurd_sigthread_stack_base;
extern unsigned long int __hurd_sigthread_stack_end;

#endif	/* hurd/threadvar.h */
