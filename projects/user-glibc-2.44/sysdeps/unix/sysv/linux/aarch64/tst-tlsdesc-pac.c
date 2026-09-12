/* AArch64 tests for unwinding TLSDESC (BZ 32612)
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

#include <stdlib.h>
#include <unwind.h>
#include <support/xdlfcn.h>

static _Unwind_Reason_Code
unwind_callback (struct _Unwind_Context* context, void* closure)
{
  return _URC_NO_REASON;
}

/* Assume that TLS variable from tst-tlsdesc-pac-mod.so will trigger
   the slow-path that allocates the required memory with malloc.  */
void *
malloc (size_t s)
{
  _Unwind_Backtrace (unwind_callback, NULL);
  return calloc (1, s);
}

static int
do_test (void)
{
  void *h = xdlopen ("tst-tlsdesc-pac-mod.so", RTLD_LAZY);
  void (*func)(void) = xdlsym (h, "bar");
  func ();

  return 0;
}

#include <support/test-driver.c>
