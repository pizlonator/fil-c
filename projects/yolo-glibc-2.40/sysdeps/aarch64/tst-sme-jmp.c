/* Test for SME longjmp.
   Copyright (C) 2023 Free Software Foundation, Inc.
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

struct blk {
  void *za_save_buffer;
  uint16_t num_za_save_slices;
  char __reserved[6];
};

static unsigned long svl;
static uint8_t *za_orig;
static uint8_t *za_dump;
static uint8_t *za_save;

static unsigned long
get_svl (void)
{
  register unsigned long x0 asm ("x0");
  asm volatile (
    ".inst   0x04bf5820  /* rdsvl   x0, 1  */\n"
    : "=r" (x0));
  return x0;
}

/* PSTATE.ZA = 1, set ZA state to active.  */
static void
start_za (void)
{
  asm volatile (
    ".inst   0xd503457f  /* smstart za  */");
}

/* Read SVCR to get SM (bit0) and ZA (bit1) state.  */
static unsigned long
get_svcr (void)
{
  register unsigned long x0 asm ("x0");
  asm volatile (
    ".inst   0xd53b4240  /* mrs     x0, svcr  */\n"
    : "=r" (x0));
  return x0;
}

/* Load data into ZA byte by byte from p.  */
static void __attribute__ ((noinline))
load_za (const void *p)
{
  register unsigned long x15 asm ("x15") = 0;
  register unsigned long x16 asm ("x16") = (unsigned long)p;
  register unsigned long x17 asm ("x17") = svl;

  asm volatile (
    ".inst   0xd503437f  /* smstart sm  */\n"
    ".L_ldr_loop:\n"
    ".inst   0xe1006200  /* ldr     za[w15, 0], [x16]  */\n"
    "add     w15, w15, 1\n"
    ".inst   0x04305030  /* addvl   x16, x16, 1  */\n"
    "cmp     w15, w17\n"
    "bne     .L_ldr_loop\n"
    ".inst   0xd503427f  /* smstop  sm  */\n"
    : "+r"(x15), "+r"(x16), "+r"(x17));
}

/* Set tpidr2 to BLK.  */
static void
set_tpidr2 (struct blk *blk)
{
  register unsigned long x0 asm ("x0") = (unsigned long)blk;
  asm volatile (
    ".inst   0xd51bd0a0  /* msr     tpidr2_el0, x0  */\n"
    :: "r"(x0) : "memory");
}

/* Returns tpidr2.  */
static void *
get_tpidr2 (void)
{
  register unsigned long x0 asm ("x0");
  asm volatile (
    ".inst   0xd53bd0a0  /* mrs     x0, tpidr2_el0  */\n"
    : "=r"(x0) :: "memory");
  return (void *) x0;
}

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
  load_za (za_orig);

  print_data ("za save space", za_save);
  p = get_tpidr2 ();
  printf ("before setjmp: tp2 = %p\n", p);
  if (p != &blk)
    FAIL_EXIT1 ("tpidr2 is not set to BLK %p", (void *)&blk);
  if (setjmp (env) == 0)
    {
      p = get_tpidr2 ();
      printf ("before longjmp: tp2 = %p\n", p);
      if (p != &blk)
	FAIL_EXIT1 ("tpidr2 is clobbered");
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
  load_za (za_orig);

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
