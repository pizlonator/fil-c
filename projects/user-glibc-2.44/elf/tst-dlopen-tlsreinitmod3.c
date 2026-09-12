/* Test that dlopen preserves already accessed TLS (bug 31717), module 3.
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

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

/* Used to verify from the main program that the test ran.  */
bool tlsreinitmod3_tested;

/* This TLS variable must not revert back to the initial state after
   dlopen.  */
static __thread int tlsreinitmod3_state = 1;

/* Set from the ELF constructor during dlopen.  */
static bool tlsreinitmod3_constructed;

/* Second half of test, behind a compiler barrier.  The compiler
   barrier is necessary to prevent carrying over TLS address
   information from call_tlsreinitmod3 to call_tlsreinitmod3_tail.  */
void call_tlsreinitmod3_tail (void *self) __attribute__ ((weak));

/* Called from tst-dlopen-tlsreinitmod2.so.  */
void
call_tlsreinitmod3 (void)
{
  printf ("info: call_tlsreinitmod3 invoked (state=%d)\n",
          tlsreinitmod3_state);

  if (tlsreinitmod3_constructed)
    {
      puts ("error: call_tlsreinitmod3 called after ELF constructor");
      fflush (stdout);
      /* Cannot rely on test harness due to dynamic linking.  */
      _exit (1);
    }

  tlsreinitmod3_state = 2;

  /* Self-dlopen.  This will run the ELF constructor.   */
  void *self = dlopen ("tst-dlopen-tlsreinitmod3.so", RTLD_NOW);
  if (self == NULL)
    {
      printf ("error: dlopen: %s\n", dlerror ());
      fflush (stdout);
      /* Cannot rely on test harness due to dynamic linking.  */
      _exit (1);
    }

  call_tlsreinitmod3_tail (self);
}

void
call_tlsreinitmod3_tail (void *self)
{
  printf ("info: dlopen returned in tlsreinitmod3 (state=%d)\n",
          tlsreinitmod3_state);

  if (!tlsreinitmod3_constructed)
    {
      puts ("error: dlopen did not call tlsreinitmod3 ELF constructor");
      fflush (stdout);
      /* Cannot rely on test harness due to dynamic linking.  */
      _exit (1);
    }

  if (tlsreinitmod3_state != 2)
    {
      puts ("error: TLS state reverted in tlsreinitmod3");
      fflush (stdout);
      /* Cannot rely on test harness due to dynamic linking.  */
      _exit (1);
    }

  dlclose (self);

  /* Signal test completion to the main program.  */
  tlsreinitmod3_tested = true;
}

static void __attribute__ ((constructor))
tlsreinitmod3_init (void)
{
  puts ("info: constructor of tst-dlopen-tlsreinitmod3.so invoked");
  tlsreinitmod3_constructed = true;
}
