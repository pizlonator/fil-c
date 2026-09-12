/* Test the state save/restore procedures during signal handling when an
   interruptible RPC is restarted.

   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <mach/message.h>
#include <mach/gnumach.h>
#include <mach/mach_traps.h>
#include <mach/mig_errors.h>
#include <mach-shortcuts.h>
#include <mach_init.h>
#include <hurd/io.h>
#include <hurd/io_reply.h>

#include <support/check.h>
#include <support/xthread.h>

#include "test-xstate.h"

void handler (int signum, siginfo_t *info, void *context)
{
  printf ("signal %d setting a different CPU state\n", signum);
  char mmxbuf3[MMXSTATE_BUFFER_SIZE];
  char xbuf3[XSTATE_BUFFER_SIZE];
  memset (mmxbuf3, 0x77, MMXSTATE_BUFFER_SIZE);
  memset (xbuf3, 0x77, XSTATE_BUFFER_SIZE);
  SET_MMXSTATE (mmxbuf3);
  SET_XSTATE (xbuf3);
}

static const mach_msg_type_t RetCodeCheck = {
  .msgt_name =            (unsigned char) MACH_MSG_TYPE_INTEGER_32,
  .msgt_size =            32,
  .msgt_number =          1,
  .msgt_inline =          TRUE,
  .msgt_longform =        FALSE,
  .msgt_deallocate =      FALSE,
  .msgt_unused =          0
};


/* Helper thread to simulate a proper RPC interruption during dignal handling */
void* fake_interruptor (void *arg)
{
  int err;
  sigset_t ss;
  TEST_COMPARE (sigemptyset (&ss), 0);
  TEST_COMPARE (sigaddset (&ss, SIGUSR1), 0);
  TEST_COMPARE (sigprocmask (SIG_BLOCK, &ss, NULL), 0);

  struct {
    mach_msg_header_t Head;
  } request;
  mach_port_t rxport = *((mach_port_t*)arg);
  err = mach_msg (&request.Head, MACH_RCV_MSG, 0, sizeof (request), rxport,
                  MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  TEST_COMPARE (err, MACH_MSG_SUCCESS);
  TEST_COMPARE (request.Head.msgh_bits, 0x1112);
  TEST_COMPARE (request.Head.msgh_size, sizeof (request.Head));
  TEST_COMPARE (request.Head.msgh_id, 33000);

  mig_reply_header_t reply;
  reply.Head = request.Head;
  reply.Head.msgh_id += 100;
  reply.RetCodeType = RetCodeCheck;
  reply.RetCode = KERN_SUCCESS;
  err = mach_msg (&reply.Head, MACH_SEND_MSG, sizeof (reply), 0, MACH_PORT_NULL,
                  MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  TEST_COMPARE (err, MACH_MSG_SUCCESS);

  return NULL;
}


/* Helper thread to send a signal to the main thread in the middle of
 * an interruptible rpc */
void* signal_sender (void *arg)
{
  int err;
  sigset_t ss;
  TEST_COMPARE (sigemptyset (&ss), 0);
  TEST_COMPARE (sigaddset (&ss, SIGUSR1), 0);
  TEST_COMPARE (sigprocmask (SIG_BLOCK, &ss, NULL), 0);

  /* Receive the first request, we won't answer to this. */
  struct {
    mach_msg_header_t head;
    char data[64];
  } m1, m2;
  mach_port_t rxport = *((mach_port_t*)arg);
  memset (&m1, 0, sizeof (m1));
  memset (&m2, 0, sizeof (m2));
  err = mach_msg (&m1.head, MACH_RCV_MSG, 0, sizeof (m1), rxport,
                  MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  TEST_COMPARE (err, MACH_MSG_SUCCESS);

  /* interrupt the ongoing rpc with a signal, using the
   * interruptible rpc protocol */
  pthread_t thintr = xpthread_create (NULL, fake_interruptor, arg);
  TEST_COMPARE (kill (getpid (), SIGUSR1), 0);
  xpthread_join (thintr);

  /* Complete the interruption by sending EINTR */
  mig_reply_header_t reply;
  reply.Head = m1.head;
  reply.Head.msgh_id += 100;
  reply.RetCodeType = RetCodeCheck;
  reply.RetCode = EINTR;
  err = mach_msg (&reply.Head, MACH_SEND_MSG, sizeof (reply), 0, MACH_PORT_NULL,
                  MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  TEST_COMPARE (err, MACH_MSG_SUCCESS);

  /* Receive the retried rpc, and check that it has the same payload
   * as the first one. Port names might still be different. */
  err = mach_msg (&m2.head, MACH_RCV_MSG, 0, sizeof (m2), rxport,
                  MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  TEST_COMPARE (m1.head.msgh_bits, m2.head.msgh_bits);
  TEST_COMPARE (m1.head.msgh_size, m2.head.msgh_size);
  TEST_COMPARE (m1.head.msgh_id, m2.head.msgh_id);
  TEST_COMPARE_BLOB (m1.data, sizeof (m1.data), m2.data, sizeof (m2.data));

  /* And finally make the rpc succeed by sending a valid reply */
  err = io_read_reply (m2.head.msgh_remote_port, MACH_MSG_TYPE_MOVE_SEND_ONCE,
                       KERN_SUCCESS, NULL, 0);
  TEST_COMPARE (err, MACH_MSG_SUCCESS);

  return NULL;
}


static int do_test (void)
{
#if ! XSTATE_HELPERS_SUPPORTED
  FAIL_UNSUPPORTED ("Test not supported on this arch.");
#endif

  /* Setup signal handling; we need to handle the signal in the main
   * thread, the other ones will explicitely block SIGUSR1. */
  struct sigaction act = { 0 };
  act.sa_flags = SA_RESTART;
  act.sa_sigaction = &handler;
  TEST_COMPARE (sigaction (SIGUSR1, &act, NULL), 0);

  mach_port_t fakeio;
  int err;
  err = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE, &fakeio);
  TEST_COMPARE (err, MACH_MSG_SUCCESS);

  err = mach_port_insert_right (mach_task_self (), fakeio, fakeio,
                                MACH_MSG_TYPE_MAKE_SEND);
  TEST_COMPARE (err, MACH_MSG_SUCCESS);

  pthread_t thsender = xpthread_create (NULL, signal_sender, &fakeio);

  char *buf;
  mach_msg_type_number_t n;
  TEST_COMPARE (io_read (fakeio, &buf, &n, 1, 2), 0);

  xpthread_join (thsender);
  return EXIT_SUCCESS;
}

#include <support/test-driver.c>
