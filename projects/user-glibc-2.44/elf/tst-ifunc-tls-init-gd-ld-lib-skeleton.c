/* Shared-library skeleton for tst-ifunc-tls-init-gd-ld.
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

/* Unlike tst-ifunc-tls-init-lib-skeleton.c, which uses initial-exec and
   resolves via a single TP-relative load, this skeleton's TLS_MODEL is
   either "global-dynamic" or "local-dynamic" so the resolver's read of
   'sentinel' must traverse __tls_get_addr (or the architecture's
   TLSDESC equivalent).  That dynamic-TLS access can lazily allocate a
   per-module TLS block, which is the path being exercised.  */

#include <sys/cdefs.h>

#ifndef TLS_MODEL
# error "tst-ifunc-tls-init-gd-ld-lib-skeleton.c needs TLS_MODEL defined"
#endif

/* Without static the relocation against the SENTINEL goes through the regular
   global-symbol lookup path; combined with TLS_MODEL="global-dynamic" this
   exercises the "global GD" variant rather than the file-local one.  */
#ifndef SENTINEL_STORAGE
# define SENTINEL_STORAGE static
#endif

#define SENTINEL 0x5A5A1234

SENTINEL_STORAGE volatile __thread int sentinel
  __attribute__ ((tls_model (TLS_MODEL))) = SENTINEL;
static volatile int last_seen_sentinel;

static int
impl_ok (void)
{
  return SENTINEL;
}

static int
impl_bad (void)
{
  return 0;
}

int
get_last_seen_sentinel (void)
{
  return last_seen_sentinel;
}

static typeof (impl_ok) *
__attribute_used__
resolver (void)
{
  int s = sentinel;
  last_seen_sentinel = s;
  return s == SENTINEL ? impl_ok : impl_bad;
}
int ifunc_tls (void) __attribute__ ((ifunc ("resolver")));

int (*fptr) (void) = ifunc_tls;
