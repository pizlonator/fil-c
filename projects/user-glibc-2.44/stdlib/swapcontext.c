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

int
swapcontext (ucontext_t *oucp, const ucontext_t *ucp)
{
  if (!oucp->__uc_fiber_context)
    {
      oucp->__uc_fiber_context = zfiber_context_new ();
      zfiber_context_bind_sigset (oucp->__uc_fiber_context, &oucp->uc_sigmask);
    }
  zfiber_context_swapcontext (oucp->__uc_fiber_context, ucp->__uc_fiber_context);
  return 0;
}


stub_warning (swapcontext)
