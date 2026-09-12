/* Shared library for tst-ifunc-tls-write.
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

/* The DSO defines an initial-exec thread-local 'counter' initialised to
   SENTINEL, the IFUNC resolver (via the non-PLT IFUNC reloc on 'fptr')
   overwrites counter with MARKER, and the test then reads counter back
   through a getter.  */

#include <sys/cdefs.h>

#define SENTINEL 0x5A5A1234u
#define MARKER   0x32125A5Au

/* The 'volatile' avoids constant fold optimization in impl_ok.  */
static volatile __thread unsigned int counter
  __attribute__ ((tls_model ("initial-exec"))) = SENTINEL;

static unsigned int
impl (void)
{
  return 0;
}

static typeof (impl) *
__attribute_used__
resolver (void)
{
  counter = MARKER;
  return impl;
}
unsigned int ifunc_write (void) __attribute__ ((ifunc ("resolver")));

/* Force the resolver rather than lazy bind on first call, which would
   re-write counter after the test reads it).  Using a COPY'd fptr also lets
   the test verify the resolver ran without making a PLT call that would
   itself fire the resolver again.  */
unsigned int (*fptr) (void) = ifunc_write;

unsigned int
get_counter (void)
{
  return counter;
}
