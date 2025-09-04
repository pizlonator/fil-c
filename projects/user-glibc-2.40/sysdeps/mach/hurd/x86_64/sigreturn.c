/* Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

#include <hurd.h>
#include <hurd/signal.h>
#include <hurd/msg.h>
#include <stdlib.h>

/* This is run on the thread stack after restoring it, to be able to
   unlock SS off sigstack.  */
void
__sigreturn2 (struct hurd_sigstate *ss, uintptr_t *usp,
              mach_port_t sc_reply_port)
{
  mach_port_t reply_port;
  _hurd_sigstate_unlock (ss);

  /* Destroy the MiG reply port used by the signal handler, and restore the
     reply port in use by the thread when interrupted.

     We cannot use the original reply port for our RPCs that we do here, since
     we could unexpectedly receive/consume a reply message meant for the user
     (in particular, msg_sig_post_reply), and also since we would deallocate
     the port if *our* RPC fails, which we don't want to do since the user
     still has the old name.  And so, temporarily set MACH_PORT_DEAD as our
     reply name, and make sure destroying the port is the very last RPC we
     do.  */
  reply_port = THREAD_GETMEM (THREAD_SELF, reply_port);
  THREAD_SETMEM (THREAD_SELF, reply_port, MACH_PORT_DEAD);
  if (__glibc_likely (MACH_PORT_VALID (reply_port)))
    (void) __mach_port_mod_refs (__mach_task_self (), reply_port,
                                 MACH_PORT_RIGHT_RECEIVE, -1);
  THREAD_SETMEM (THREAD_SELF, reply_port, sc_reply_port);

  asm volatile (
                /* Point the stack to the register dump.  */
                "movq %0, %%rsp\n"
                /* Pop off the registers.  */
                "popq %%r8\n"
                "popq %%r9\n"
                "popq %%r10\n"
                "popq %%r11\n"
                "popq %%r12\n"
                "popq %%r13\n"
                "popq %%r14\n"
                "popq %%r15\n"
                "popq %%rdi\n"
                "popq %%rsi\n"
                "popq %%rbp\n"
                "popq %%rbx\n"
                "popq %%rdx\n"
                "popq %%rcx\n"
                "popq %%rax\n"
                "popfq\n"
                /* Restore %rip and %rsp with a single instruction.  */
                "retq $128" :
                : "rm" (usp));
  __builtin_unreachable ();
}

int
__sigreturn (struct sigcontext *scp)
{
  struct hurd_sigstate *ss;
  struct hurd_userlink *link = (void *) &scp[1];
  uintptr_t *usp;
  mach_port_t sc_reply_port;

  if (__glibc_unlikely (scp == NULL || (scp->sc_mask & _SIG_CANT_MASK)))
    return __hurd_fail (EINVAL);

  ss = _hurd_self_sigstate ();
  _hurd_sigstate_lock (ss);

  /* Remove the link on the `active resources' chain added by
     _hurd_setup_sighandler.  Its purpose was to make sure
     that we got called; now we have, it is done.  */
  _hurd_userlink_unlink (link);

  /* Restore the set of blocked signals, and the intr_port slot.  */
  ss->blocked = scp->sc_mask;
  ss->intr_port = scp->sc_intr_port;

  /* Check for pending signals that were blocked by the old set.  */
  if (_hurd_sigstate_pending (ss) & ~ss->blocked)
    {
      /* There are pending signals that just became unblocked.  Wake up the
	 signal thread to deliver them.  But first, squirrel away SCP where
	 the signal thread will notice it if it runs another handler, and
	 arrange to have us called over again in the new reality.  */
      ss->context = scp;
      _hurd_sigstate_unlock (ss);
      __msg_sig_post (_hurd_msgport, 0, 0, __mach_task_self ());
      /* If a pending signal was handled, sig_post never returned.
	 If it did return, the pending signal didn't run a handler;
	 proceed as usual.  */
      _hurd_sigstate_lock (ss);
      ss->context = NULL;
    }

  if (scp->sc_onstack)
    ss->sigaltstack.ss_flags &= ~SS_ONSTACK;

  if (scp->sc_fpused)
    /* Restore the FPU state.  Mach conveniently stores the state
       in the format the i387 `frstor' instruction uses to restore it.  */
    asm volatile ("frstor %0" : : "m" (scp->sc_fpsave));

  /* Copy the registers onto the user's stack, to be able to release the
     altstack (by unlocking sigstate).  Note that unless an altstack is used,
     the sigcontext will itself be located on the user's stack, so we may well
     be overwriting it here (or later in __sigreturn2).

     So: do this very carefully.  First, load sc_reply_port, which is the only
     other bit of sigcontext that __sigreturn2 needs.  Then copy the registers
     without reordering them, but skipping the ones we won't need.  We have to
     copy starting from the larger addresses down, since our register dump is
     located at a larger address than the sigcontext.  */

  sc_reply_port = scp->sc_reply_port;
  usp = (uintptr_t *) (scp->sc_ursp - 128);

  *--usp = scp->sc_rip;
  *--usp = scp->sc_rfl;
  *--usp = scp->sc_rax;
  *--usp = scp->sc_rcx;
  *--usp = scp->sc_rdx;
  *--usp = scp->sc_rbx;
  *--usp = scp->sc_rbp;
  *--usp = scp->sc_rsi;
  *--usp = scp->sc_rdi;
  *--usp = scp->sc_r15;
  *--usp = scp->sc_r14;
  *--usp = scp->sc_r13;
  *--usp = scp->sc_r12;
  *--usp = scp->sc_r11;
  *--usp = scp->sc_r10;
  *--usp = scp->sc_r9;
  *--usp = scp->sc_r8;

  /* Switch to the user's stack that we have just prepared, and call
     __sigreturn2.  Clobber "memory" to make sure GCC flushes the stack
     setup to actual memory.  We align the stack as per the ABI, but pass
     the original usp to __sigreturn2 as an argument.  */
  asm volatile ("movq %1, %%rsp\n"
                "andq $-16, %%rsp\n"
                "call __sigreturn2" :
                : "D" (ss), "S" (usp), "d" (sc_reply_port)
                : "memory");
  __builtin_unreachable ();
}

weak_alias (__sigreturn, sigreturn)
