/* Shared library for tst-ifunc-plt.
   Two static IFUNCs whose resolvers both call get_value() via PLT.
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


/* Both resolvers call get_value() via PLT (one JUMP_SLOT entry in
   .rel{a}.plt).  This verifies that every IRELATIVE entry is deferred
   until after .rela.plt has been processed, not just the first one.  */

#include <sys/cdefs.h>
#include <stddef.h>

extern int get_value (void);

static int
impl_a (void)
{
  return 1;
}

static int
impl_b (void)
{
  return 2;
}

static typeof (impl_a) *
__attribute_used__
resolve_a (void)
{
  return get_value () == 42 ? impl_a : NULL;
}

static typeof (impl_b) *
__attribute_used__
resolve_b (void)
{
  return get_value () == 42 ? impl_b : NULL;
}

/* The test is only built for $(have-ifunc), so we can assume HAVE_GCC_IFUNC
   here.  */
int compute_a (void) __attribute__ ((ifunc ("resolve_a")));
int compute_b (void) __attribute__ ((ifunc ("resolve_b")));
