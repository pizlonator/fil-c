/* Test that ZA state of SME is cleared in both parent and child
   when clone3() syscall is used.
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

#include "tst-sme-skeleton.c"

#include <clone3.h>

#include <errno.h>
#include <signal.h>
#include <support/xsched.h>

/* Since clone3 is not a public symbol, we link this test explicitly
   with clone3.o and have to provide this declaration.  */
int __clone3 (struct clone_args *cl_args, size_t size,
	    int (*func)(void *arg), void *arg);

static int
fun (void * const arg)
{
  printf ("in child: %s\n", (const char *)arg);
  /* Check that ZA state of SME was disabled in child.  */
  check_sme_za_state ("after clone3 in child", /* Clear.  */ true);
  return 0;
}

static char __attribute__((aligned(16)))
stack[1024 * 1024];

/* Required by __arm_za_disable.o and provided by the startup code
   as a hidden symbol.  */
uint64_t _dl_hwcap2;

static void
run (struct blk *ptr)
{
  _dl_hwcap2 = getauxval (AT_HWCAP2);

  char *syscall_name = (char *)"clone3";
  struct clone_args args = {
    .flags = CLONE_VM | CLONE_VFORK,
    .exit_signal = SIGCHLD,
    .stack = (uintptr_t) stack,
    .stack_size = sizeof (stack),
  };
  printf ("in parent: before %s\n", syscall_name);

  /* Enabled ZA state so that effect of disabling be observable.  */
  enable_sme_za_state (ptr);
  check_sme_za_state ("before clone3", /* Clear.  */ false);

  pid_t pid = __clone3 (&args, sizeof (args), fun, syscall_name);

  /* Check that ZA state of SME was disabled in parent.  */
  check_sme_za_state ("after clone3 in parent", /* Clear.  */ true);

  printf ("%s child pid: %d\n", syscall_name, pid);

  xwaitpid (pid, NULL, 0);
  printf ("in parent: after %s\n", syscall_name);
}

/* Workaround to simplify linking with clone3.o.  */
void __syscall_error(int code)
{
  int err = -code;
  fprintf (stderr, "syscall error %d (%s)\n", err, strerror (err));
  exit (err);
}
