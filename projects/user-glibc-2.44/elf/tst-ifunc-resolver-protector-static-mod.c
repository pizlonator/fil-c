/* Stack-protector-instrumented IFUNC resolver used by
   tst-ifunc-resolver-protector-static.  BZ #34164.
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

/* This translation unit is built with -fstack-protector-all so that the
   compiler instruments the resolver below.  The resolver's prologue loads
   the canary from the TCB (TCB-canary ABIs) or from __stack_chk_guard
   (global-var ABIs); the epilogue compares it against the on-stack copy.
   Pre-fix the prologue load either faulted (TCB-canary) or loaded zero
   (global-var).

   The resolver also records the canary value via STACK_CHK_GUARD so the
   test can verify it matches what main observes.  */

#include <stackguard-macros.h>
#include <stdint.h>
#include <tls.h>

#define SENTINEL 0x5A5A1234

static volatile uintptr_t resolver_canary;

uintptr_t
get_resolver_canary (void)
{
  return resolver_canary;
}

static int
impl_ok (int x)
{
  return x + SENTINEL;
}

typedef int (*fn_t) (int);

static fn_t
__attribute_used__
resolver (void)
{
  /* Buffer forces -fstack-protector-all to emit canary code even with
     no other reason to.  */
  volatile char buf[32];
  for (unsigned i = 0; i < sizeof (buf); ++i)
    buf[i] = 0;

  resolver_canary = STACK_CHK_GUARD;

  return impl_ok;
}

int compute (int) __attribute__ ((ifunc ("resolver")));

int (*fptr) (int) = compute;
