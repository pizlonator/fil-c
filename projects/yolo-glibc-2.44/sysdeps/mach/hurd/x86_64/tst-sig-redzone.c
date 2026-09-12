/* Test the redzone is not affected during signal handling.

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


#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>

#include <mach/message.h>
#include <mach/gnumach.h>
#include <mach/mach_traps.h>
#include <mach-shortcuts.h>
#include <mach_init.h>
#include <hurd/io.h>
#include <hurd/io_reply.h>

#include <support/check.h>
#include <support/xthread.h>

static volatile atomic_bool startflag = ATOMIC_VAR_INIT (false);
static volatile atomic_bool loopflag = ATOMIC_VAR_INIT (true);

void handler (int signum, siginfo_t *info, void *context)
{
  char buf[128];
  memset (buf, 0x77, sizeof buf);
  printf ("signal %d hurting stack a bit\n", signum);
  atomic_store_explicit (&loopflag, false, memory_order_release);
}

/* Helper thread to send a signal to the main thread  */
void* signal_sender (void *arg)
{
  sigset_t ss;
  assert (! sigemptyset (&ss));
  assert (! sigaddset (&ss, SIGUSR1));
  assert (! sigprocmask (SIG_BLOCK, &ss, NULL));

  while (!atomic_load_explicit (&startflag, memory_order_acquire))
    ;
  TEST_COMPARE (kill (getpid (), SIGUSR1), 0);

  return NULL;
}

static int do_test (void)
{
  struct sigaction act = { 0 };
  act.sa_sigaction = &handler;
  TEST_COMPARE (sigaction (SIGUSR1, &act, NULL), 0);

  pthread_t thsender = xpthread_create (NULL, signal_sender, NULL);

  int check_redzone (void);
  TEST_COMPARE (check_redzone (), 0);

  xpthread_join (thsender);
  return EXIT_SUCCESS;
}

asm (
".text\n"
".align 16\n"
".type check_redzone, @function\n"
"check_redzone:\n"
"	.cfi_startproc\n"

"	movabs	$0x3333333333333333, %rax\n"
"	mov	$(128/8), %ecx\n"
"	lea	-128(%rsp), %rdi\n"
"rep	stosq\n"

"	movl	$1, startflag(%rip)\n"
"wait:\n"
"	cmpl	$0, loopflag(%rip)\n"
"	jne	wait\n"

"	movabs	$0x3333333333333333, %rax\n"
"	mov	$(128/8), %ecx\n"
"	lea	-128(%rsp), %rdi\n"
"repe	scasq\n"
"	jne	fail\n"

"	xor	%eax,%eax\n"
"	ret\n"
"fail:\n"
"	movq	$1,%rax\n"
"	ret\n"

"	.cfi_endproc\n"
"	.size	check_redzone, .-check_redzone\n"
".previous\n"
);

#include <support/test-driver.c>
