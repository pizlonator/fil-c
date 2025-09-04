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

register int *sp asm ("%esp");

#include <hurd.h>
#include <hurd/signal.h>
#include <hurd/msg.h>
#include <stdlib.h>
#include <string.h>

/* This is run on the thread stack after restoring it, to be able to
   unlock SS off sigstack.  */
static void
__sigreturn2 (int *usp, struct sigcontext *scp)
{
  mach_port_t reply_port;
  struct hurd_sigstate *ss;

  /* We know the sigstate must be initialized by the call below, but the
     compiler does not.  Help it out a little bit by eliding the check that
     _hurd_self_sigstate makes otherwise.  */
  ss = THREAD_GETMEM (THREAD_SELF, _hurd_sigstate);
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
  THREAD_SETMEM (THREAD_SELF, reply_port, scp->sc_reply_port);

  sp = usp;
#define A(line) asm volatile (#line)
  /* The members in the sigcontext are arranged in this order
     so we can pop them easily.  */

  /* Pop the segment registers (except %cs and %ss, done last).  */
  A (popl %gs);
  A (popl %fs);
  A (popl %es);
  A (popl %ds);
  /* Pop the general registers.  */
  A (popa);
  /* Pop the processor flags.  */
  A (popf);
  /* Return to the saved PC.  */
  A (ret);

  /* Firewall.  */
  A (hlt);
#undef A
  __builtin_unreachable ();
}

int
__sigreturn (struct sigcontext *scp)
{
  struct hurd_sigstate *ss;
  struct hurd_userlink *link = (void *) &scp[1];

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

  {
    /* There are convenient instructions to pop state off the stack, so we
       copy the registers onto the user's stack, switch there, pop and
       return.  */

    int usp_arg, *usp = (int *) scp->sc_uesp;

    *--usp = scp->sc_eip;
    *--usp = scp->sc_efl;
    memcpy (usp -= 12, &scp->sc_i386_thread_state, 12 * sizeof (int));
    usp_arg = (int) usp;

    *--usp = (int) scp;
    /* Pass usp to __sigreturn2 so it can unwind itself easily.  */
    *--usp = usp_arg;
    /* Bogus return address for __sigreturn2 */
    *--usp = 0;
    *--usp = (int) __sigreturn2;

    /* Restore thread stack */
    sp = usp;
    /* Return into __sigreturn2.  */
    asm volatile ("ret");
    /* Firewall.  */
    asm volatile ("hlt");
  }

  /* NOTREACHED */
  return -1;
}

weak_alias (__sigreturn, sigreturn)
