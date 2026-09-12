/* Test the state save/restore procedures during signal handling.

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

#include "test-xstate.h"

static volatile atomic_bool startflag = ATOMIC_VAR_INIT (false);
static volatile atomic_bool loopflag = ATOMIC_VAR_INIT (true);
static volatile bool doublesig = false;

void handler (int signum, siginfo_t *info, void *context)
{
  char mmxbuf3[MMXSTATE_BUFFER_SIZE];
  char xbuf3[XSTATE_BUFFER_SIZE];
  memset (mmxbuf3, 0x77, MMXSTATE_BUFFER_SIZE);
  memset (xbuf3, 0x77, XSTATE_BUFFER_SIZE);
  SET_MMXSTATE (mmxbuf3);
  SET_XSTATE (xbuf3);
  printf ("signal %d setting a different CPU state\n", signum);
  if (doublesig)
    {
      doublesig = false;
      TEST_COMPARE (kill (getpid (), SIGUSR1), 0);
    }
  else
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
#if ! XSTATE_HELPERS_SUPPORTED
  FAIL_UNSUPPORTED ("Test not supported on this arch.");
#endif

  struct sigaction act = { 0 };
  act.sa_sigaction = &handler;
  TEST_COMPARE (sigaction (SIGUSR1, &act, NULL), 0);

  pthread_t thsender = xpthread_create (NULL, signal_sender, NULL);

  char mmxbuf1[MMXSTATE_BUFFER_SIZE], mmxbuf2[MMXSTATE_BUFFER_SIZE];
  char xbuf1[XSTATE_BUFFER_SIZE], xbuf2[XSTATE_BUFFER_SIZE];
  memset (mmxbuf1, 0x33, MMXSTATE_BUFFER_SIZE);
  memset (xbuf1, 0x33, XSTATE_BUFFER_SIZE);

  SET_MMXSTATE (mmxbuf1);
  SET_XSTATE (xbuf1);

  atomic_store_explicit (&startflag, true, memory_order_release);
  while (atomic_load_explicit (&loopflag, memory_order_acquire))
    ;

  GET_MMXSTATE (mmxbuf2);
  GET_XSTATE (xbuf2);
  TEST_COMPARE_BLOB (mmxbuf1, sizeof (mmxbuf1), mmxbuf2, sizeof (mmxbuf2));
  TEST_COMPARE_BLOB (xbuf1, sizeof (xbuf1), xbuf2, sizeof (xbuf2));

  xpthread_join (thsender);

  /* Now retry with double signalling.  */
  doublesig = true;
  atomic_store_explicit (&startflag, false, memory_order_relaxed);
  atomic_store_explicit (&loopflag, true, memory_order_relaxed);

  thsender = xpthread_create (NULL, signal_sender, NULL);

  SET_MMXSTATE (mmxbuf1);
  SET_XSTATE (xbuf1);

  atomic_store_explicit (&startflag, true, memory_order_release);
  while (atomic_load_explicit (&loopflag, memory_order_acquire))
    ;

  GET_MMXSTATE (mmxbuf2);
  GET_XSTATE (xbuf2);
  TEST_COMPARE_BLOB (mmxbuf1, sizeof (mmxbuf1), mmxbuf2, sizeof (mmxbuf2));
  TEST_COMPARE_BLOB (xbuf1, sizeof (xbuf1), xbuf2, sizeof (xbuf2));

  xpthread_join (thsender);

  return EXIT_SUCCESS;
}

#include <support/test-driver.c>
