/* Test child for parent backtrace test.
   Copyright (C) 2013, 2016 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Command line syntax: ./backtrace-child [--ptraceme|--gencore]
   --ptraceme will call ptrace (PTRACE_TRACEME) in the two threads.
   --gencore will call abort () at its end.
   Main thread will signal SIGUSR2.  Other thread will signal SIGUSR1.
   There used to be a difference between x86_64 and other architectures.
   To test getting a signal at the very first instruction of a function:
     PC will get changed to function 'jmp' by backtrace.c function
     prepare_thread.  Then SIGUSR2 will be signalled to backtrace-child
     which will invoke function sigusr2.
     This is all done so that signal interrupts execution of the very first
     instruction of a function.  Properly handled unwind should not slip into
     the previous unrelated function.
     The tested functionality is arch-independent but the code reproducing it
     has to be arch-specific.
   On non-x86_64:
     sigusr2 gets called by normal function call from function stdarg.
   On any arch then sigusr2 calls raise (SIGUSR1) for --ptraceme.
   abort () is called otherwise, expected for --gencore core dump.

   Expected x86_64 output:
   TID 10276:
   # 0 0x7f7ab61e9e6b      raise
   # 1 0x7f7ab661af47 - 1  main
   # 2 0x7f7ab5e3bb45 - 1  __libc_start_main
   # 3 0x7f7ab661aa09 - 1  _start
   TID 10278:
   # 0 0x7f7ab61e9e6b      raise
   # 1 0x7f7ab661ab3c - 1  sigusr2
   # 2 0x7f7ab5e4fa60      __restore_rt
   # 3 0x7f7ab661ab47      jmp
   # 4 0x7f7ab661ac92 - 1  stdarg
   # 5 0x7f7ab661acba - 1  backtracegen
   # 6 0x7f7ab661acd1 - 1  start
   # 7 0x7f7ab61e2c53 - 1  start_thread
   # 8 0x7f7ab5f0fdbd - 1  __clone

   Expected non-x86_64 (i386) output; __kernel_vsyscall are skipped if found:
   TID 10408:
   # 0 0xf779f430          __kernel_vsyscall
   # 1 0xf7771466 - 1      raise
   # 2 0xf77c1d07 - 1      main
   # 3 0xf75bd963 - 1      __libc_start_main
   # 4 0xf77c1761 - 1      _start
   TID 10412:
   # 0 0xf779f430          __kernel_vsyscall
   # 1 0xf7771466 - 1      raise
   # 2 0xf77c18f4 - 1      sigusr2
   # 3 0xf77c1a10 - 1      stdarg
   # 4 0xf77c1a2c - 1      backtracegen
   # 5 0xf77c1a48 - 1      start
   # 6 0xf77699da - 1      start_thread
   # 7 0xf769bbfe - 1      __clone

   But the raise jmp patching was unreliable. It depends on the CFI for the raise()
   function in glibc to be the same as for the jmp() function. This is not always
   the case. Some newer glibc versions rewrote raise() and now the CFA is calculated
   differently. So we disable raise jmp patching everywhere.
   */

#ifdef __x86_64__
/* #define RAISE_JMP_PATCHING 1 */
#endif

#include <config.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#ifndef __linux__

int
main (int argc __attribute__ ((unused)), char **argv)
{
  fprintf (stderr, "%s: Unwinding not supported for this architecture\n",
           argv[0]);
  return 77;
}

#else /* __linux__ */
#include <sys/ptrace.h>
#include <signal.h>

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define NOINLINE_NOCLONE __attribute__ ((noinline, noclone))
#else
#define NOINLINE_NOCLONE __attribute__ ((noinline))
#endif

#define NORETURN __attribute__ ((noreturn))
#define UNUSED __attribute__ ((unused))
#define USED __attribute__ ((used))

static int ptraceme, gencore;

/* Execution will arrive here from jmp by an artificial ptrace-spawn signal.  */

static NOINLINE_NOCLONE void
sigusr2 (int signo)
{
  assert (signo == SIGUSR2);
  if (! gencore)
    {
      raise (SIGUSR1);
      /* Do not return as stack may be invalid due to ptrace-patched PC to the
	 jmp function.  */
      pthread_exit (NULL);
      /* Not reached.  */
      abort ();
    }
  /* Here we dump the core for --gencore.  */
  raise (SIGABRT);
  /* Avoid tail call optimization for the raise call.  */
  asm volatile ("");
}

static NOINLINE_NOCLONE void
dummy1 (void)
{
  asm volatile ("");
}

#ifdef RAISE_JMP_PATCHING
static NOINLINE_NOCLONE USED void
jmp (void)
{
  /* Not reached, signal will get ptrace-spawn to jump into sigusr2.  */
  abort ();
}
#endif

static NOINLINE_NOCLONE void
dummy2 (void)
{
  asm volatile ("");
}

static NOINLINE_NOCLONE NORETURN void
stdarg (int f UNUSED, ...)
{
  sighandler_t sigusr2_orig = signal (SIGUSR2, sigusr2);
  assert (sigusr2_orig == SIG_DFL);
  errno = 0;
  if (ptraceme)
    {
      long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      assert (l == 0);
    }
#ifdef RAISE_JMP_PATCHING
  if (! gencore)
    {
      /* Execution will get PC patched into function jmp.  */
      raise (SIGUSR1);
    }
#endif
  sigusr2 (SIGUSR2);
  /* Not reached.  */
  abort ();
}

static NOINLINE_NOCLONE void
dummy3 (void)
{
  asm volatile ("");
}

static NOINLINE_NOCLONE void
backtracegen (void)
{
  stdarg (1);
  /* Here should be no instruction after the stdarg call as it is noreturn
     function.  It must be stdarg so that it is a call and not jump (jump as
     a tail-call).  */
}

static NOINLINE_NOCLONE void
dummy4 (void)
{
  asm volatile ("");
}

static void *
start (void *arg UNUSED)
{
  backtracegen ();
  /* Not reached.  */
  abort ();
}

int
main (int argc UNUSED, char **argv)
{
  setbuf (stdout, NULL);
  assert (*argv++);
  ptraceme = (*argv && strcmp (*argv, "--ptraceme") == 0);
  argv += ptraceme;
  gencore = (*argv && strcmp (*argv, "--gencore") == 0);
  argv += gencore;
  assert (!*argv);
  /* These dummy* functions are there so that each of their surrounding
     functions has some unrelated code around.  The purpose of some of the
     tests is verify unwinding the very first / after the very last instruction
     does not inappropriately slip into the unrelated code around.  */
  dummy1 ();
  dummy2 ();
  dummy3 ();
  dummy4 ();
  if (gencore)
    printf ("%ld\n", (long) getpid ());
  pthread_t thread;
  int i = pthread_create (&thread, NULL, start, NULL);
  // pthread_* functions do not set errno.
  assert (i == 0);
  if (ptraceme)
    {
      errno = 0;
      long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      assert (l == 0);
    }
  if (gencore)
    pthread_join (thread, NULL);
  else
    raise (SIGUSR2);
  return 0;
}

#endif /* ! __linux__ */

