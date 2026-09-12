extern struct mutex _hurd_siglock; /* Locks _hurd_sigstates.  */

#ifndef	_HURD_SIGNAL_H
extern struct hurd_sigstate *_hurd_self_sigstate (void) __attribute__ ((__const__));
#ifndef _ISOMAC
libc_hidden_proto (_hurd_self_sigstate)
#endif

extern int _hurd_sigstate_is_global_rcv (const struct hurd_sigstate *ss);

#include_next <hurd/signal.h>

#ifndef _ISOMAC

#include <bits/types/error_t.h>
extern error_t __sigthreadmask (struct hurd_sigstate *ss, int how,
				const sigset_t *set, sigset_t *oset, int clear_pending);

#if defined __USE_EXTERN_INLINES
# if IS_IN (libc) || IS_IN (libpthread)
#  include <sigsetops.h>
#  include <tls.h>
# endif
#endif

#ifndef _HURD_SIGNAL_H_EXTERN_INLINE
#define _HURD_SIGNAL_H_EXTERN_INLINE __extern_inline
#endif

#if defined __USE_EXTERN_INLINES && IS_IN (libc)
_HURD_SIGNAL_H_EXTERN_INLINE struct hurd_sigstate *
_hurd_self_sigstate (void)
{
  struct hurd_sigstate *ss = THREAD_GETMEM (THREAD_SELF, _hurd_sigstate);
  if (__glibc_unlikely (ss == NULL))
    {
      thread_t self = __mach_thread_self ();

      /* The thread variable is unset; this must be the first time we've
        asked for it.  In this case, the critical section flag cannot
        possible already be set.  Look up our sigstate structure the slow
        way.  */
      ss = _hurd_thread_sigstate (self);
      THREAD_SETMEM (THREAD_SELF, _hurd_sigstate, ss);
      __mach_port_deallocate (__mach_task_self (), self);
    }
  return ss;
}

_HURD_SIGNAL_H_EXTERN_INLINE void *
_hurd_critical_section_lock (void)
{
  struct hurd_sigstate *ss;

  if (__LIBC_NO_TLS ())
    /* TLS is currently initializing, no need to enter critical section.  */
    return NULL;

  ss = _hurd_self_sigstate ();

  if (! __spin_try_lock (&ss->critical_section_lock))
    /* We are already in a critical section, so do nothing.  */
    return NULL;

  /* With the critical section lock held no signal handler will run.
     Return our sigstate pointer; this will be passed to
     _hurd_critical_section_unlock to unlock it.  */
  return ss;
}

/* Check whether SS is a global receiver.  */
_HURD_SIGNAL_H_EXTERN_INLINE int
_hurd_sigstate_is_global_rcv (const struct hurd_sigstate *ss)
{
  extern struct hurd_sigstate *_hurd_global_sigstate;

  return (_hurd_global_sigstate != NULL)
	 && (ss->actions[0].sa_handler == SIG_IGN);
}

_HURD_SIGNAL_H_EXTERN_INLINE void
_hurd_critical_section_unlock (void *our_lock)
{
  if (our_lock == NULL)
    /* The critical section lock was held when we began.  Do nothing.  */
    return;
  else
    {
      /* It was us who acquired the critical section lock.  Unlock it.  */
      struct hurd_sigstate *ss = (struct hurd_sigstate *) our_lock;
      sigset_t pending;
      _hurd_sigstate_lock (ss);
      __spin_unlock (&ss->critical_section_lock);
      pending = _hurd_sigstate_pending (ss) & ~ss->blocked;
      _hurd_sigstate_unlock (ss);
      if (__glibc_unlikely (!__sigisemptyset (&pending)))
	/* There are unblocked signals pending, which weren't
	   delivered because we were in the critical section.
	   Tell the signal thread to deliver them now.  */
	__msg_sig_post (_hurd_msgport, 0, 0, __mach_task_self ());
    }
}

/* Lock/unlock a hurd_sigstate structure.  If the accessors below require
   it, the global sigstate will be locked as well.  */
_HURD_SIGNAL_H_EXTERN_INLINE void
_hurd_sigstate_lock (struct hurd_sigstate *ss)
{
  if (_hurd_sigstate_is_global_rcv (ss))
    __spin_lock (&_hurd_global_sigstate->lock);
  __spin_lock (&ss->lock);
}

_HURD_SIGNAL_H_EXTERN_INLINE void
_hurd_sigstate_unlock (struct hurd_sigstate *ss)
{
  __spin_unlock (&ss->lock);
  if (_hurd_sigstate_is_global_rcv (ss))
    __spin_unlock (&_hurd_global_sigstate->lock);
}
#endif /* defined __USE_EXTERN_INLINES && IS_IN (libc) */


libc_hidden_proto (_hurd_exception2signal)
libc_hidden_proto (_hurd_intr_rpc_mach_msg)
libc_hidden_proto (_hurd_thread_sigstate)
libc_hidden_proto (_hurd_raise_signal)
libc_hidden_proto (_hurd_sigstate_set_global_rcv)
libc_hidden_proto (_hurd_sigstate_is_global_rcv)
libc_hidden_proto (_hurd_sigstate_lock)
libc_hidden_proto (_hurd_sigstate_pending)
libc_hidden_proto (_hurd_sigstate_unlock)
libc_hidden_proto (_hurd_sigstate_delete)
#endif
#ifdef _HURD_SIGNAL_H_HIDDEN_DEF
libc_hidden_def (_hurd_self_sigstate)
libc_hidden_def (_hurd_sigstate_is_global_rcv)
libc_hidden_def (_hurd_sigstate_lock)
libc_hidden_def (_hurd_sigstate_unlock)
#endif
#endif
