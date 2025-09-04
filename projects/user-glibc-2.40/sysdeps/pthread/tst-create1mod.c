/* Verify that pthread_create does not deadlock when ctors take locks.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

/* Require TLS setup for the module.  */
__thread int tlsvar;

void ctor (void);
void dtor (void);

static void __attribute__ ((constructor))
do_init (void)
{
  dprintf (1, "constructor started: %d.\n", tlsvar++);
  ctor ();
  dprintf (1, "constructor done: %d.\n", tlsvar++);
}

static void __attribute__ ((destructor))
do_end (void)
{
  dprintf (1, "destructor started: %d.\n", tlsvar++);
  dtor ();
  dprintf (1, "destructor done: %d.\n", tlsvar++);
}
