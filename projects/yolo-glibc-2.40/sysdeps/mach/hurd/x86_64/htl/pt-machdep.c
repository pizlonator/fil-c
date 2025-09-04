/* Machine dependent pthreads code.  Hurd/x86_64 version.
   Copyright (C) 2000-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library;  if not, see
   <https://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <assert.h>

#include <mach.h>
#include <mach/machine/thread_status.h>
#include <mach/x86_64/mach_i386.h>
#include <mach/mig_errors.h>
#include <mach/thread_status.h>

int
__thread_set_pcsptp (thread_t thread,
                     int set_ip, void *ip,
                     int set_sp, void *sp,
                     int set_tp, void *tp)
{
  error_t err;
  struct i386_thread_state state;
  struct i386_fsgs_base_state fsgs_state;
  mach_msg_type_number_t state_count;

  state_count = i386_THREAD_STATE_COUNT;

  err = __thread_get_state (thread, i386_REGS_SEGS_STATE,
                            (thread_state_t) &state, &state_count);
  if (err)
    return err;
  assert (state_count == i386_THREAD_STATE_COUNT);

  if (set_sp)
    state.ursp = (uintptr_t) sp;
  if (set_ip)
    state.rip = (uintptr_t) ip;

  err = __thread_set_state (thread, i386_REGS_SEGS_STATE,
                            (thread_state_t) &state, i386_THREAD_STATE_COUNT);
  if (err)
    return err;

  if (set_tp)
    {
      state_count = i386_FSGS_BASE_STATE_COUNT;
      err = __thread_get_state (thread, i386_FSGS_BASE_STATE,
                                (thread_state_t) &fsgs_state, &state_count);
      if (err)
        return err;
      assert (state_count == i386_FSGS_BASE_STATE_COUNT);
      fsgs_state.fs_base = (uintptr_t) tp;
      err = __thread_set_state (thread, i386_FSGS_BASE_STATE,
                                (thread_state_t) &fsgs_state, state_count);
      if (err)
        return err;
    }

  return 0;
}
