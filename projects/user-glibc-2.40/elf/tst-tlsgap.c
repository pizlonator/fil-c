/* TLS modid gap reuse regression test for bug 29039.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <dlfcn.h>
#include <pthread.h>
#include <support/xdlfcn.h>
#include <support/xthread.h>
#include <support/check.h>

static void *mod[3];
#define MOD(i) "tst-tlsgap-mod" #i ".so"
static const char *modname[3] = { MOD(0), MOD(1), MOD(2) };
#undef MOD

static void
open_mod (int i)
{
  mod[i] = xdlopen (modname[i], RTLD_LAZY);
  printf ("open %s\n", modname[i]);
}

static void
close_mod (int i)
{
  xdlclose (mod[i]);
  mod[i] = NULL;
  printf ("close %s\n", modname[i]);
}

static void
access_mod (int i, const char *sym)
{
  int *(*f) (void) = xdlsym (mod[i], sym);
  int *p = f ();
  printf ("access %s: %s() = %p\n", modname[i], sym, p);
  TEST_VERIFY_EXIT (p != NULL);
  ++*p;
}

static void *
start (void *arg)
{
  /* The DTV generation is at the last dlopen of mod0 and the
     entry for mod1 is NULL.  */

  open_mod (1); /* Reuse modid of mod1. Uses dynamic TLS.  */

  /* DTV is unchanged: dlopen only updates the DTV to the latest
     generation if static TLS is allocated for a loaded module.

     With bug 29039, the TLSDESC relocation in mod1 uses the old
     dlclose generation of mod1 instead of the new dlopen one so
     DTV is not updated on TLS access.  */

  access_mod (1, "f1");

  return arg;
}

static int
do_test (void)
{
  open_mod (0);
  open_mod (1);
  open_mod (2);
  close_mod (0);
  close_mod (1); /* Create modid gap at mod1.  */
  open_mod (0); /* Reuse modid of mod0, bump generation count.  */

  /* Create a thread where DTV of mod1 is NULL.  */
  pthread_t t = xpthread_create (NULL, start, NULL);
  xpthread_join (t);
  return 0;
}

#include <support/test-driver.c>
