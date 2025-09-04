/* Check LD_AUDIT with static TLS.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <ctype.h>
#include <stdlib.h>
#include <link.h>

#define tls_ie __attribute__ ((tls_model ("initial-exec")))

__thread int tls_var0 tls_ie;
__thread int tls_var1 tls_ie = 0x10;

/* Defined at tst-auditmod21b.so  */
extern __thread int tls_var2;
extern __thread int tls_var3;

static volatile int out;

static void
call_libc (void)
{
  /* isspace accesses the initial-exec glibc TLS variables, which are
     setup in glibc initialization.  */
  out = isspace (' ');
}

unsigned int
la_version (unsigned int v)
{
  tls_var0 = 0x1;
  if (tls_var1 != 0x10)
    abort ();
  tls_var1 = 0x20;

  tls_var2 = 0x2;
  if (tls_var3 != 0x20)
    abort ();
  tls_var3 = 0x40;

  call_libc ();

  return LAV_CURRENT;
}

unsigned int
la_objopen (struct link_map* map, Lmid_t lmid, uintptr_t* cookie)
{
  call_libc ();
  *cookie = (uintptr_t) map;
  return 0;
}

void
la_activity (uintptr_t* cookie, unsigned int flag)
{
  if (tls_var0 != 0x1 || tls_var1 != 0x20)
    abort ();
  call_libc ();
}

void
la_preinit (uintptr_t* cookie)
{
  call_libc ();
}
