/* Copyright (C) 1998-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <ucontext.h>
#include <pizlonated_runtime.h>
#include <stdlib.h>

int
getcontext (ucontext_t *ucp)
{
  ucp->__uc_fiber_context = zfiber_context_new ();
  zfiber_context_bind_sigset (ucp->__uc_fiber_context, &ucp->uc_sigmask);
  zfiber_context_getcontext (ucp->__uc_fiber_context);
  ucp->uc_link = NULL;
  ucp->uc_stack.ss_sp = NULL;
  ucp->uc_stack.ss_flags = 0;
  ucp->uc_stack.ss_size = 0;
  return 0;
}


stub_warning (getcontext)
