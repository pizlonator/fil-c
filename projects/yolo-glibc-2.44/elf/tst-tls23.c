/* Test that __tls_get_addr preserves caller-saved registers.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
#include <tst-tls23.h>

#ifndef IS_SUPPORTED
# define IS_SUPPORTED() true
#endif

/* An architecture can define it to clobber caller-saved registers in
   malloc below to verify that __tls_get_addr won't change caller-saved
   registers.  */
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

static void *mod;
static const char *modname = "tst-tls23-mod.so";

static void
open_mod (void)
{
  mod = xdlopen (modname, RTLD_LAZY);
  printf ("open %s\n", modname);
}

static void
close_mod (void)
{
  xdlclose (mod);
  mod = NULL;
  printf ("close %s\n", modname);
}

static void
access_mod (const char *sym)
{
  struct tls var = { -4, -4, -4, -4 };
  struct tls *(*f) (struct tls *) = xdlsym (mod, sym);
  /* Check that our malloc is called.  */
  malloc_counter = 0;
  struct tls *p = f (&var);
  TEST_VERIFY (malloc_counter != 0);
  printf ("access %s: %s() = %p\n", modname, sym, p);
  TEST_VERIFY_EXIT (memcmp (p, &var, sizeof (var)) == 0);
  ++(p->a);
}

static void *
start (void *arg)
{
  access_mod ("apply_tls");
  return arg;
}

static int
do_test (void)
{
  if (!IS_SUPPORTED ())
    return EXIT_UNSUPPORTED;

  open_mod ();
  pthread_t t = xpthread_create (NULL, start, NULL);
  xpthread_join (t);
  close_mod ();

  return 0;
}

#include <support/test-driver.c>
