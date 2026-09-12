/* Implementation of the test verification functions for memory access
   checks.
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

#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <support/xsignal.h>

#include "check_mem_access.h"

static sigjmp_buf sigsegv_jmp_buf;

static void
__sigsegv_handler (int signum)
{
  siglongjmp (sigsegv_jmp_buf, signum);
}

bool check_mem_access (const void *addr, bool write)
{
  /* This is obviously not thread-safe.  */
  static bool handler_set_up;
  if (!handler_set_up)
    {
      struct sigaction sa = {
	.sa_handler = __sigsegv_handler,
	.sa_flags = SA_NODEFER,
      };
      sigemptyset (&sa.sa_mask);
      xsigaction (SIGSEGV, &sa, NULL);
      /* Some system generates SIGBUS accessing the guard area when it is
	 setup with madvise.  */
      xsigaction (SIGBUS, &sa, NULL);
      handler_set_up = true;
    }
  int r = sigsetjmp (sigsegv_jmp_buf, 0);
  if (r == 0)
    {
      if (write)
        *(volatile char *)addr = 'x';
      else
        *(volatile char *)addr;
      return true;
    }
  if (r == SIGSEGV || r == SIGBUS)
    return false;
  return true;
}
