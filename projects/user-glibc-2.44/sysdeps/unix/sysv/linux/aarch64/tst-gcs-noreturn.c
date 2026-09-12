/* AArch64 test for GCS abort when returning to non-GCS address.
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

#include "tst-gcs-helper.h"

#include <sys/prctl.h>
#include <stdlib.h>

#include <support/xsignal.h>

# ifndef PR_SET_SHADOW_STACK_STATUS
#  define PR_SET_SHADOW_STACK_STATUS	75
#  define PR_SHADOW_STACK_ENABLE	(1UL << 0)
# endif

static void
run_with_gcs (void)
{
  int r = prctl (PR_SET_SHADOW_STACK_STATUS, PR_SHADOW_STACK_ENABLE, 0, 0, 0);
  /* Syscall should succeed.  */
  TEST_VERIFY (r == 0);
  bool gcs_enabled = __check_gcs_status ();
  /* Now GCS should be enabled.  */
  TEST_VERIFY (gcs_enabled);
  printf ("GCS is %s\n", gcs_enabled ? "enabled" : "disabled");
}

static struct _aarch64_ctx *
extension (void *p)
{
  return p;
}

#ifndef GCS_MAGIC
#define GCS_MAGIC 0x47435300
#endif

static void
handler (int sig, siginfo_t *si, void *ctx)
{
  TEST_VERIFY (sig == SIGSEGV);
  ucontext_t *uc = ctx;
  void *p = uc->uc_mcontext.__reserved;
  if (extension (p)->magic == FPSIMD_MAGIC)
    p = (char *)p + extension (p)->size;
  if (extension (p)->magic == GCS_MAGIC)
    {
      struct { uint64_t x, gcspr, y, z; } *q = p;
      printf ("GCS pointer: %016lx\n", q->gcspr);
      exit (0);
    }
  else
    exit (3);
}

static int
do_test (void)
{
  /* Check if GCS could possible by enabled.  */
  if (!(getauxval (AT_HWCAP) & HWCAP_GCS))
    FAIL_UNSUPPORTED ("kernel or CPU does not support GCS");
  bool gcs_enabled = __check_gcs_status ();
  /* This test should be rung with GCS initially disabled.  */
  TEST_VERIFY (!gcs_enabled);

  /* We can't use EXPECTED_SIGNAL because of cases when
     this test runs on a system that does not support GCS
     which is being detected at runtime.  */
  struct sigaction sigact;
  sigemptyset (&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigact.sa_flags = sigact.sa_flags | SA_SIGINFO;
  sigact.sa_sigaction = handler;
  xsigaction (SIGSEGV, &sigact, NULL);

  run_with_gcs ();
  /* If we reached this point, then something went wrong.
     Returning from a function that enabled GCS should result in
     SIGSEGV that we catch with the handler set up above.  */
  return 2;
}

#include <support/test-driver.c>
