/* BZ 28817: TLS read from a static-pie IFUNC resolver.
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

#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>

__thread int bar;
extern __thread int bar_gd asm ("bar")
  __attribute__ ((tls_model("global-dynamic")));
static int *bar_ptr;

int foo (void);

static void
init_foo (void)
{
  bar_ptr = &bar_gd;
}

static int
my_foo (void)
{
  return bar_ptr != NULL;
}

static __typeof (foo) *
inhibit_stack_protector
__attribute_used__
foo_ifunc (void)
{
  init_foo ();
  __typeof (foo) *res = my_foo;
  return res;
};
__typeof (foo) foo __attribute__ ((ifunc ("foo" "_ifunc")));

static int
do_test (void)
{
  TEST_VERIFY (foo ());
  TEST_VERIFY (&bar == bar_ptr);
  return 0;
}

#include <support/test-driver.c>
