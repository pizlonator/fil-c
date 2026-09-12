/* Test TLSDESC relocation.
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
   <http://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <support/xdlfcn.h>
#include <support/xthread.h>
#include <support/check.h>
#include <support/test-driver.h>
#include <tst-gnu2-tls2.h>

#ifndef IS_SUPPORTED
# define IS_SUPPORTED() true
#endif

/* An architecture can define it to clobber caller-saved registers in
   malloc below to verify that the implicit TLSDESC call won't change
   caller-saved registers.  */
#ifndef PREPARE_MALLOC
# define PREPARE_MALLOC()
#endif

extern void * __libc_malloc (size_t);

size_t malloc_counter = 0;

void *
malloc (size_t n)
{
  PREPARE_MALLOC ();
  malloc_counter++;
  return __libc_malloc (n);
}

static void *mod[3];
#ifndef MOD
# define MOD(i) "tst-gnu2-tls2mod" #i ".so"
#endif
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
  struct tls var = { -4, -4, -4, -4 };
  struct tls *(*f) (struct tls *) = xdlsym (mod[i], sym);
  /* Check that our malloc is called.  */
  malloc_counter = 0;
  struct tls *p = f (&var);
  TEST_VERIFY (malloc_counter != 0);
  printf ("access %s: %s() = %p\n", modname[i], sym, p);
  TEST_VERIFY_EXIT (memcmp (p, &var, sizeof (var)) == 0);
  ++(p->a);
}

static void *
start (void *arg)
{
  /* The DTV generation is at the last dlopen of mod0 and the
     entry for mod1 is NULL.  */

  open_mod (1); /* Reuse modid of mod1. Uses dynamic TLS.  */

  /* Force the slow path in GNU2 TLS descriptor call.  */
  access_mod (1, "apply_tls");

  return arg;
}

static int
do_test (void)
{
  if (!IS_SUPPORTED ())
    return EXIT_UNSUPPORTED;

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
