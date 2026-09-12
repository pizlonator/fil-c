/* Wait for process state.
   Copyright (C) 2020-2026 Free Software Foundation, Inc.
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

#ifndef SUPPORT_PROCESS_STATE_H
#define SUPPORT_PROCESS_STATE_H

#include <sys/types.h>

enum support_process_state
{
  support_process_state_running      = 0x01,  /* R (running).  */
  support_process_state_sleeping     = 0x02,  /* S (sleeping).  */
  support_process_state_disk_sleep   = 0x04,  /* D (disk sleep).  */
  support_process_state_stopped      = 0x08,  /* T (stopped).  */
  support_process_state_tracing_stop = 0x10,  /* t (tracing stop).  */
  support_process_state_dead         = 0x20,  /* X (dead).  */
  support_process_state_zombie       = 0x40,  /* Z (zombie).  */
  support_process_state_parked       = 0x80,  /* P (parked).  */
  support_process_state_invalid      = 0x100  /* Invalid state.  */
};

/* Wait for process PID to reach state STATE.  It can be a combination of
   multiple possible states ('process_state_running | process_state_sleeping')
   where the function return when any of these state are observed.
   For an invalid state not represented by SUPPORT_PROCESS_STATE, it fallbacks
   to a 2 second sleep.
   Return the found process state.  */
enum support_process_state
support_process_state_wait (pid_t pid, enum support_process_state state);

/* Same as support_process_state_wait, but wait for the task TID (obtained
   with gettid) from the current process.
   NB: this function does not guard against TID reuse (the kernel might
   assign the TID to a different thread between the gettid and the function
   call if the thread exits and another is created).  It is the caller's
   responsibility to ensure the call is safe to use.  */
enum support_process_state
support_thread_state_wait (pid_t pid, enum support_process_state state);

#endif
