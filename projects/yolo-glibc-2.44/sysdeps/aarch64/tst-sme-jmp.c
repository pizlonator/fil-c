/* Test for SME longjmp.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ucontext.h>
#include <sys/auxv.h>
#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>

#include "tst-sme-helper.h"

/* Streaming SVE vector register size.  */
static unsigned long svl;

static uint8_t *za_orig;
static uint8_t *za_dump;
static uint8_t *za_save;

static void
print_data(const char *msg, void *p)
{
  unsigned char *a = p;
  printf ("%s:\n", msg);
  for (int i = 0; i < svl; i++)
    {
      printf ("%d: ", i);
      for (int j = 0; j < svl; j++)
	printf("%02x,", a[i*svl+j]);
      printf("\n");
    }
  printf(".\n");
  fflush (stdout);
}

__attribute__ ((noinline))
static void
do_longjmp (jmp_buf env)
{
  longjmp (env, 1);
}

__attribute__ ((noinline))
static void
do_setcontext (const ucontext_t *p)
{
  setcontext (p);
}

static void
longjmp_test (void)
{
  unsigned long svcr;
  jmp_buf env;
  void *p;
  int r;
  struct blk blk = {za_save, svl, {0}};

  printf ("longjmp test:\n");
  p = get_tpidr2 ();
  printf ("initial tp2 = %p\n", p);
  if (p != NULL)
    FAIL_EXIT1 ("tpidr2 is not initialized to 0");
  svcr = get_svcr ();
  if (svcr != 0)
    FAIL_EXIT1 ("svcr != 0: %lu", svcr);
  set_tpidr2 (&blk);
  start_za ();
  load_za (za_orig, svl);

  print_data ("za save space", za_save);
  p = get_tpidr2 ();
  printf ("before setjmp: tp2 = %p\n", p);
  if (p != &blk)
    FAIL_EXIT1 ("tpidr2 is not set to BLK %p", (void *)&blk);
  if (setjmp (env) == 0)
    {
      p = get_tpidr2 ();
      printf ("before longjmp: tp2 = %p\n", p);
      if (p != NULL)
	FAIL_EXIT1 ("tpidr2 has not been reset to null");
      do_longjmp (env);
      FAIL_EXIT1 ("longjmp returned");
    }
  p = get_tpidr2 ();
  printf ("after longjmp: tp2 = %p\n", p);
  if (p != NULL)
    FAIL_EXIT1 ("tpidr2 is not set to 0");
  svcr = get_svcr ();
  if (svcr != 0)
    FAIL_EXIT1 ("svcr != 0: %lu", svcr);
  print_data ("za save space", za_save);
  r = memcmp (za_orig, za_save, svl*svl);
  if (r != 0)
    FAIL_EXIT1 ("saving za failed");
}

static void
setcontext_test (void)
{
  volatile int setcontext_done = 0;
  unsigned long svcr;
  ucontext_t ctx;
  void *p;
  int r;
  struct blk blk = {za_save, svl, {0}};

  printf ("setcontext test:\n");
  p = get_tpidr2 ();
  printf ("initial tp2 = %p\n", p);
  if (p != NULL)
    FAIL_EXIT1 ("tpidr2 is not initialized to 0");
  svcr = get_svcr ();
  if (svcr != 0)
    FAIL_EXIT1 ("svcr != 0: %lu", svcr);
  set_tpidr2 (&blk);
  start_za ();
  load_za (za_orig, svl);

  print_data ("za save space", za_save);
  p = get_tpidr2 ();
  printf ("before getcontext: tp2 = %p\n", p);
  if (p != &blk)
    FAIL_EXIT1 ("tpidr2 is not set to BLK %p", (void *)&blk);
  r = getcontext (&ctx);
  if (r != 0)
    FAIL_EXIT1 ("getcontext failed");
  if (setcontext_done == 0)
    {
      p = get_tpidr2 ();
      printf ("before setcontext: tp2 = %p\n", p);
      if (p != &blk)
	FAIL_EXIT1 ("tpidr2 is clobbered");
      setcontext_done = 1;
      do_setcontext (&ctx);
      FAIL_EXIT1 ("setcontext returned");
    }
  p = get_tpidr2 ();
  printf ("after setcontext: tp2 = %p\n", p);
  if (p != NULL)
    FAIL_EXIT1 ("tpidr2 is not set to 0");
  svcr = get_svcr ();
  if (svcr != 0)
    FAIL_EXIT1 ("svcr != 0: %lu", svcr);
  print_data ("za save space", za_save);
  r = memcmp (za_orig, za_save, svl*svl);
  if (r != 0)
    FAIL_EXIT1 ("saving za failed");
}

static int
do_test (void)
{
  unsigned long hwcap2;

  hwcap2 = getauxval (AT_HWCAP2);
  if ((hwcap2 & HWCAP2_SME) == 0)
    return EXIT_UNSUPPORTED;

  svl = get_svl ();
  printf ("svl: %lu\n", svl);
  if (svl < 16 || svl % 16 != 0 || svl >= (1 << 16))
    FAIL_EXIT1 ("invalid svl");

  za_orig = xmalloc (svl*svl);
  za_save = xmalloc (svl*svl);
  za_dump = xmalloc (svl*svl);
  memset (za_orig, 1, svl*svl);
  memset (za_save, 2, svl*svl);
  memset (za_dump, 3, svl*svl);
  for (int i = 0; i < svl; i++)
    for (int j = 0; j < svl; j++)
      za_orig[i*svl+j] = i*svl+j;
  print_data ("original data", za_orig);

  longjmp_test ();

  memset (za_save, 2, svl*svl);
  memset (za_dump, 3, svl*svl);

  setcontext_test ();

  free (za_orig);
  free (za_save);
  free (za_dump);
  return 0;
}

#include <support/test-driver.c>
