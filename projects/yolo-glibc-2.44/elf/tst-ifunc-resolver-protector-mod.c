/* Stack-protector-instrumented IFUNC resolver in a shared library, used
   by tst-ifunc-resolver-protector.  BZ #27582.
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

/* Built with -fstack-protector-all so the resolver's prologue/epilogue
   carries the full canary check.  Reaching the return statement means the
   canary load did not fault (TCB-canary ABIs) and the canary compare did not
   call __stack_chk_fail.  The dynamic linker calls security_init() before the
   main relocation loop, so resolvers fired from startup or dlopen should
   always observe an initialised canary; this test guards against any future
   reordering that would break that invariant.  */

#include <sys/cdefs.h>

#define SENTINEL 0x5A5A1234

static volatile int resolver_ran;

int
get_resolver_ran (void)
{
  return resolver_ran;
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
  /* Buffer + zero-fill force -fstack-protector-all canary code.  */
  volatile char buf[32];
  for (unsigned i = 0; i < sizeof (buf); ++i)
    buf[i] = 0;
  resolver_ran = 1;
  return impl_ok;
}

int compute (int) __attribute__ ((ifunc ("resolver")));

/* Address taken in DSO data to force an R_*_IRELATIVE in .rela.dyn (a
   non-PLT relocation against the IFUNC).  */
int (*fptr) (int) = compute;
