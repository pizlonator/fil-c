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

struct makecontext_args
{
  ucontext_t *ucp;
  void (*func) (void);
  int argc;
  void* first_real_arg;
};

static void
makecontext_trampoline(void)
{
  struct makecontext_args *args = (struct makecontext_args *) zcallee_closure_data();
  zcall (args->func, &args->first_real_arg);
  ucontext_t* link = args->ucp->uc_link;
  if (!link)
    exit(0);
  setcontext (link);
}

void
makecontext (ucontext_t *ucp, void (*func) (void), int argc, ...)
{
  zfiber_context_makecontext (ucp->__uc_fiber_context,
                              ucp->uc_stack.ss_size,
                              zclosure_new (makecontext_trampoline, zargs ()));
}


stub_warning (makecontext)
