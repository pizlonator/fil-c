/* Shared library for tst-ifunc-tls-init-dlopen.
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

/* Same as tst-ifunc-tls-init-lib.c, but exercises the dlopen path.  */

#define SENTINEL 0x5A5A1234

static volatile __thread int sentinel
  __attribute__ ((tls_model ("initial-exec"))) = SENTINEL;
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

static int (*resolver (void)) (void)
{
  int s = sentinel;
  last_seen_sentinel = s;
  return s == SENTINEL ? impl_ok : impl_bad;
}

int ifunc_tls (void) __attribute__ ((ifunc ("resolver")));

/* Force a non-PLT relocation against the IFUNC symbol.  */
int (*fptr) (void) = ifunc_tls;
