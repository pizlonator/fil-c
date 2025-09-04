/* pthread_atfork supports handlers that call pthread_atfork or dlclose.
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

#include <pthread.h>
#include <stdlib.h>

/* This dynamically loaded library simply registers its atfork handlers when
   asked to.  The atfork handlers should never be executed because the
   library is unloaded before fork is called by the test program.  */

static void
prepare (void)
{
  abort ();
}

static void
parent (void)
{
  abort ();
}

static void
child (void)
{
  abort ();
}

void
reg_atfork_handlers (void)
{
  pthread_atfork (prepare, parent, child);
}
