/* Test that __libc_arm_za_disable aborts on unknown TPIDR2 extensions.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <signal.h>
#include <stdint.h>
#include <sys/auxv.h>

#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/test-driver.h>

#include "tst-sme-helper.h"

extern void __libc_arm_za_disable (void);

/* Required by __arm_za_disable.o and provided by the startup code
   as a hidden symbol.  */
uint64_t _dl_hwcap2;

static struct blk blk = {
  .za_save_buffer = NULL,
  .num_za_save_slices = 0,
  .__reserved = { 1, 0, 0, 0, 0, 0 },
};

static void
do_abort (void *closure)
{
  start_za ();
  set_tpidr2 (closure);
  __libc_arm_za_disable ();
}

static int
do_test (void)
{
  _dl_hwcap2 = getauxval (AT_HWCAP2);
  if ((_dl_hwcap2 & HWCAP2_SME) == 0)
    FAIL_UNSUPPORTED ("kernel or CPU does not support SME");

  struct support_capture_subprocess result
      = support_capture_subprocess (do_abort, &blk);
  support_capture_subprocess_check (&result, "tst-sme-za-disable-fail",
				    -SIGABRT, sc_allow_stderr);
  TEST_COMPARE_STRING (result.err.buffer,
		       "FATAL: __libc_arm_za_disable failed.\n");
  support_capture_subprocess_free (&result);

  return 0;
}

#include <support/test-driver.c>
