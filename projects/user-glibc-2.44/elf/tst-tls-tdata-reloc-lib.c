/* Shared library for tst-tls-tdata-reloc.
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

#define EXPECTED 0x12345678

static int
callee (void)
{
  return EXPECTED;
}

typedef struct
{
  int (*func) (void);
} cache_t;

/* General-dynamic TLS: the initialiser of '.func' produces an
   R_*_RELATIVE on .tdata.  */
__thread cache_t cache = { .func = callee };

int
call_through_tls (void)
{
  if (cache.func == 0)
    return -1;
  return cache.func ();
}
