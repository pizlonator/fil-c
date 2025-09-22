/* Get Dwarf Frame state for target PID or core file.
   Copyright (C) 2013, 2014 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <system.h>

#include "libdwflP.h"

/* Set STATE->pc_set from STATE->regs according to the backend.  Return true on
   success, false on error.  */
static bool
state_fetch_pc (Dwfl_Frame *state)
{
  switch (state->pc_state)
    {
    case DWFL_FRAME_STATE_PC_SET:
      return true;
    case DWFL_FRAME_STATE_PC_UNDEFINED:
      abort ();
    case DWFL_FRAME_STATE_ERROR:
      {
	Ebl *ebl = state->thread->process->ebl;
	Dwarf_CIE abi_info;
	if (ebl_abi_cfi (ebl, &abi_info) != 0)
	  {
	    __libdwfl_seterrno (DWFL_E_LIBEBL);
	    return false;
	  }
	unsigned ra = abi_info.return_address_register;
	/* dwarf_frame_state_reg_is_set is not applied here.  */
	if (ra >= ebl_frame_nregs (ebl))
	  {
	    __libdwfl_seterrno (DWFL_E_LIBEBL_BAD);
	    return false;
	  }
	state->pc = state->regs[ra] + ebl_ra_offset (ebl);
	state->pc_state = DWFL_FRAME_STATE_PC_SET;
      }
      return true;
    }
  abort ();
}

/* Do not call it on your own, to be used by thread_* functions only.  */

static void
free_states (Dwfl_Frame *state)
{
  while (state)
    {
      Dwfl_Frame *next = state->unwound;
      free(state);
      state = next;
    }
}

static Dwfl_Frame *
state_alloc (Dwfl_Thread *thread)
{
  assert (thread->unwound == NULL);
  Ebl *ebl = thread->process->ebl;
  size_t nregs = ebl_frame_nregs (ebl);
  if (nregs == 0)
    return NULL;
  assert (nregs < sizeof (((Dwfl_Frame *) NULL)->regs_set) * 8);
  Dwfl_Frame *state = malloc (sizeof (*state) + sizeof (*state->regs) * nregs);
  if (state == NULL)
    return NULL;
  state->thread = thread;
  state->signal_frame = false;
  state->initial_frame = true;
  state->pc_state = DWFL_FRAME_STATE_ERROR;
  memset (state->regs_set, 0, sizeof (state->regs_set));
  thread->unwound = state;
  state->unwound = NULL;
  return state;
}

void
internal_function
__libdwfl_process_free (Dwfl_Process *process)
{
  Dwfl *dwfl = process->dwfl;
  if (process->callbacks->detach != NULL)
    process->callbacks->detach (dwfl, process->callbacks_arg);
  assert (dwfl->process == process);
  dwfl->process = NULL;
  if (process->ebl_close)
    ebl_closebackend (process->ebl);
  free (process);
  dwfl->attacherr = DWFL_E_NOERROR;
}

/* Allocate new Dwfl_Process for DWFL.  */
static void
process_alloc (Dwfl *dwfl)
{
  Dwfl_Process *process = malloc (sizeof (*process));
  if (process == NULL)
    return;
  process->dwfl = dwfl;
  dwfl->process = process;
}

bool
dwfl_attach_state (Dwfl *dwfl, Elf *elf, pid_t pid,
		   const Dwfl_Thread_Callbacks *thread_callbacks, void *arg)
{
  if (dwfl->process != NULL)
    {
      __libdwfl_seterrno (DWFL_E_ATTACH_STATE_CONFLICT);
      return false;
    }

  /* Reset any previous error, we are just going to try again.  */
  dwfl->attacherr = DWFL_E_NOERROR;
  /* thread_callbacks is declared NN */
  if (thread_callbacks->next_thread == NULL
      || thread_callbacks->set_initial_registers == NULL)
    {
      dwfl->attacherr = DWFL_E_INVALID_ARGUMENT;
    fail:
      dwfl->attacherr = __libdwfl_canon_error (dwfl->attacherr);
      __libdwfl_seterrno (dwfl->attacherr);
      return false;
    }

  Ebl *ebl;
  bool ebl_close;
  if (elf != NULL)
    {
      ebl = ebl_openbackend (elf);
      ebl_close = true;
    }
  else
    {
      ebl = NULL;
      for (Dwfl_Module *mod = dwfl->modulelist; mod != NULL; mod = mod->next)
	{
	  /* Reading of the vDSO or (deleted) modules may fail as
	     /proc/PID/mem is unreadable without PTRACE_ATTACH and
	     we may not be PTRACE_ATTACH-ed now.  MOD would not be
	     re-read later to unwind it when we are already
	     PTRACE_ATTACH-ed to PID.  This happens when this function
	     is called from dwfl_linux_proc_attach with elf == NULL.
	     __libdwfl_module_getebl will call __libdwfl_getelf which
	     will call the find_elf callback.  */
	  if (startswith (mod->name, "[vdso: ")
	      || strcmp (strrchr (mod->name, ' ') ?: "",
			 " (deleted)") == 0)
	    continue;
	  Dwfl_Error error = __libdwfl_module_getebl (mod);
	  if (error != DWFL_E_NOERROR)
	    continue;
	  ebl = mod->ebl;
	  break;
	}
      ebl_close = false;
    }
  if (ebl == NULL)
    {
      /* Not identified EBL from any of the modules.  */
      dwfl->attacherr = DWFL_E_PROCESS_NO_ARCH;
      goto fail;
    }
  process_alloc (dwfl);
  Dwfl_Process *process = dwfl->process;
  if (process == NULL)
    {
      if (ebl_close)
	ebl_closebackend (ebl);
      dwfl->attacherr = DWFL_E_NOMEM;
      goto fail;
    }
  process->ebl = ebl;
  process->ebl_close = ebl_close;
  process->pid = pid;
  process->callbacks = thread_callbacks;
  process->callbacks_arg = arg;
  return true;
}
INTDEF(dwfl_attach_state)

pid_t
dwfl_pid (Dwfl *dwfl)
{
  if (dwfl->attacherr != DWFL_E_NOERROR)
    {
      __libdwfl_seterrno (dwfl->attacherr);
      return -1;
    }

  if (dwfl->process == NULL)
    {
      __libdwfl_seterrno (DWFL_E_NO_ATTACH_STATE);
      return -1;
    }
  return dwfl->process->pid;
}
INTDEF(dwfl_pid)

Dwfl *
dwfl_thread_dwfl (Dwfl_Thread *thread)
{
  return thread->process->dwfl;
}
INTDEF(dwfl_thread_dwfl)

pid_t
dwfl_thread_tid (Dwfl_Thread *thread)
{
  return thread->tid;
}
INTDEF(dwfl_thread_tid)

Dwfl_Thread *
dwfl_frame_thread (Dwfl_Frame *state)
{
  return state->thread;
}
INTDEF(dwfl_frame_thread)

int
dwfl_getthreads (Dwfl *dwfl, int (*callback) (Dwfl_Thread *thread, void *arg),
		 void *arg)
{
  if (dwfl->attacherr != DWFL_E_NOERROR)
    {
      __libdwfl_seterrno (dwfl->attacherr);
      return -1;
    }

  Dwfl_Process *process = dwfl->process;
  if (process == NULL)
    {
      __libdwfl_seterrno (DWFL_E_NO_ATTACH_STATE);
      return -1;
    }

  Dwfl_Thread thread;
  thread.process = process;
  thread.unwound = NULL;
  thread.callbacks_arg = NULL;
  for (;;)
    {
      thread.tid = process->callbacks->next_thread (dwfl,
						    process->callbacks_arg,
						    &thread.callbacks_arg);
      if (thread.tid < 0)
	return -1;
      if (thread.tid == 0)
	{
	  __libdwfl_seterrno (DWFL_E_NOERROR);
	  return 0;
	}
      int err = callback (&thread, arg);
      if (err != DWARF_CB_OK)
	return err;
      assert (thread.unwound == NULL);
    }
  /* NOTREACHED */
}
INTDEF(dwfl_getthreads)

struct one_arg
{
  pid_t tid;
  bool seen;
  int (*callback) (Dwfl_Thread *thread, void *arg);
  void *arg;
  int ret;
};

static int
get_one_thread_cb (Dwfl_Thread *thread, void *arg)
{
  struct one_arg *oa = (struct one_arg *) arg;
  if (! oa->seen && INTUSE(dwfl_thread_tid) (thread) == oa->tid)
    {
      oa->seen = true;
      oa->ret = oa->callback (thread, oa->arg);
      return DWARF_CB_ABORT;
    }

  return DWARF_CB_OK;
}

/* Note not currently exported, will be when there are more Dwfl_Thread
   properties to query.  Use dwfl_getthread_frames for now directly.  */
static int
getthread (Dwfl *dwfl, pid_t tid,
	   int (*callback) (Dwfl_Thread *thread, void *arg),
	   void *arg)
{
  if (dwfl->attacherr != DWFL_E_NOERROR)
    {
      __libdwfl_seterrno (dwfl->attacherr);
      return -1;
    }

  Dwfl_Process *process = dwfl->process;
  if (process == NULL)
    {
      __libdwfl_seterrno (DWFL_E_NO_ATTACH_STATE);
      return -1;
    }

  if (process->callbacks->get_thread != NULL)
    {
      Dwfl_Thread thread;
      thread.process = process;
      thread.unwound = NULL;
      thread.callbacks_arg = NULL;

      if (process->callbacks->get_thread (dwfl, tid, process->callbacks_arg,
					  &thread.callbacks_arg))
	{
	  thread.tid = tid;
	  return callback (&thread, arg);
	}

      return -1;
    }

   struct one_arg oa = { .tid = tid, .callback = callback,
			 .arg = arg, .seen = false };
   int err = INTUSE(dwfl_getthreads) (dwfl, get_one_thread_cb, &oa);

   if (err == DWARF_CB_ABORT && oa.seen)
     return oa.ret;

   if (err == DWARF_CB_OK && ! oa.seen)
     {
	errno = ESRCH;
	__libdwfl_seterrno (DWFL_E_ERRNO);
	return -1;
     }

   return err;
}

struct one_thread
{
  int (*callback) (Dwfl_Frame *frame, void *arg);
  void *arg;
};

static int
get_one_thread_frames_cb (Dwfl_Thread *thread, void *arg)
{
  struct one_thread *ot = (struct one_thread *) arg;
  return INTUSE(dwfl_thread_getframes) (thread, ot->callback, ot->arg);
}

int
dwfl_getthread_frames (Dwfl *dwfl, pid_t tid,
		       int (*callback) (Dwfl_Frame *frame, void *arg),
		       void *arg)
{
  struct one_thread ot = { .callback = callback, .arg = arg };
  return getthread (dwfl, tid, get_one_thread_frames_cb, &ot);
}
INTDEF(dwfl_getthread_frames)

int
dwfl_thread_getframes (Dwfl_Thread *thread,
		       int (*callback) (Dwfl_Frame *state, void *arg),
		       void *arg)
{
  Ebl *ebl = thread->process->ebl;
  if (ebl_frame_nregs (ebl) == 0)
    {
      __libdwfl_seterrno (DWFL_E_NO_UNWIND);
      return -1;
    }
  if (state_alloc (thread) == NULL)
    {
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return -1;
    }
  Dwfl_Process *process = thread->process;
  if (! process->callbacks->set_initial_registers (thread,
						   thread->callbacks_arg))
    {
      free_states (thread->unwound);
      thread->unwound = NULL;
      return -1;
    }
  Dwfl_Frame *state = thread->unwound;
  thread->unwound = NULL;
  if (! state_fetch_pc (state))
    {
      if (process->callbacks->thread_detach)
	process->callbacks->thread_detach (thread, thread->callbacks_arg);
      free_states (state);
      return -1;
    }
  do
    {
      int err = callback (state, arg);
      if (err != DWARF_CB_OK)
	{
	  if (process->callbacks->thread_detach)
	    process->callbacks->thread_detach (thread, thread->callbacks_arg);
	  free_states (state);
	  return err;
	}
      __libdwfl_frame_unwind (state);
      Dwfl_Frame *next = state->unwound;
      /* The old frame is no longer needed.  */
      free (state);
      state = next;
    }
  while (state && state->pc_state == DWFL_FRAME_STATE_PC_SET);

  Dwfl_Error err = dwfl_errno ();
  if (process->callbacks->thread_detach)
    process->callbacks->thread_detach (thread, thread->callbacks_arg);
  if (state == NULL || state->pc_state == DWFL_FRAME_STATE_ERROR)
    {
      free_states (state);
      __libdwfl_seterrno (err);
      return -1;
    }
  assert (state->pc_state == DWFL_FRAME_STATE_PC_UNDEFINED);
  free_states (state);
  return 0;
}
INTDEF(dwfl_thread_getframes)
