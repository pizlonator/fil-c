/* Check that a stack-protector-instrumented IFUNC resolver works in a
   static binary.  BZ #34164.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

/* When a static binary's IFUNC resolver is compiled with stack protector, the
   resolver prologue loads the canary from the TCB (TCB-canary ABIs: x86_64,
   i386, powerpc, s390) or from the global __stack_chk_guard (other ABIs).
   The test checks if TCB-canary or the global-var is properly initialized:

   1. The resolver runs without SIGSEGV (TCB-canary) or SIGABRT (canary
      mismatch).

   2. The resolver records the canary value it observed via STACK_CHK_GUARD,
      and main asserts it equals the value visible from user code -- catching
      the "silent zero canary" variant on global-var ABIs.  */

#include <stackguard-macros.h>
#include <stdint.h>
#include <tls.h>
#include <support/check.h>

#define SENTINEL 0x5A5A1234

extern int compute (int);
extern uintptr_t get_resolver_canary (void);

static int
do_test (void)
{
  /* compute() returns its argument + SENTINEL iff the resolver picked
     impl_ok, which it does whenever the canary check at the resolver
     prologue / epilogue did not abort.  */
  TEST_COMPARE (compute (1), 1 + SENTINEL);

  /* Silent-variant check: the canary the resolver loaded must match the
     canary do_test reads.  On global-var ABIs, it checks if the stack
     protector cookie is properly initialised.  */
  uintptr_t resolver_canary = get_resolver_canary ();
  uintptr_t main_canary = STACK_CHK_GUARD;
  TEST_VERIFY (resolver_canary != 0);
  TEST_COMPARE (resolver_canary, main_canary);

  return 0;
}

#include <support/test-driver.c>
