/* Test that dlopen preserves already accessed TLS (bug 31717), module 2.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

/* Defined in tst-dlopen-tlsreinitmod3.so.  This an underlinked symbol
   dependency.  */
extern void call_tlsreinitmod3 (void);

static void __attribute__ ((constructor))
tlsreinitmod2_init (void)
{
  puts ("info: constructor of tst-dlopen-tlsreinitmod2.so invoked");
  call_tlsreinitmod3 ();
}
