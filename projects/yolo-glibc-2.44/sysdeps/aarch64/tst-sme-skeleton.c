/* Template for SME tests.
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
   <https://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/auxv.h>

#include <support/check.h>
#include <support/support.h>
#include <support/xstdlib.h>
#include <support/xunistd.h>
#include <support/test-driver.h>

#include "tst-sme-helper.h"

/* Streaming SVE vector register size.  */
static unsigned long svl;

static uint8_t *state;

static void
enable_sme_za_state (struct blk *blk)
{
  start_za ();
  set_tpidr2 (blk);
  load_za (blk, svl);
}

/* Check if SME state is disabled (when CLEAR is true) or
   enabled (when CLEAR is false).  */
static void
check_sme_za_state (const char msg[], bool clear)
{
  unsigned long svcr = get_svcr ();
  void *tpidr2 = get_tpidr2 ();
  printf ("[%s]\n", msg);
  printf ("svcr = %016lx\n", svcr);
  printf ("tpidr2 = %016lx\n", (unsigned long)tpidr2);
  if (clear)
    {
      TEST_VERIFY (svcr == 0);
      TEST_VERIFY (tpidr2 == NULL);
    }
  else
    {
      TEST_VERIFY (svcr != 0);
      TEST_VERIFY (tpidr2 != NULL);
    }
}

/* Should be defined in actual test that includes this
   skeleton file. */
static void
run (struct blk *ptr);

static int
do_test (void)
{
  unsigned long hwcap2 = getauxval (AT_HWCAP2);
  if ((hwcap2 & HWCAP2_SME) == 0)
    return EXIT_UNSUPPORTED;

  /* Get current streaming SVE vector length in bytes.  */
  svl = get_svl ();
  printf ("svl: %lu\n", svl);

  TEST_VERIFY_EXIT (!(svl < 16 || svl % 16 != 0 || svl >= (1 << 16)));

  /* Initialise buffer for ZA state of SME.  */
  state = xmalloc (svl * svl);
  memset (state, 1, svl * svl);
  struct blk blk = {
    .za_save_buffer = state,
    .num_za_save_slices = svl,
    .__reserved = {0},
  };

  run (&blk);

  free (state);
  return 0;
}

#include <support/test-driver.c>
