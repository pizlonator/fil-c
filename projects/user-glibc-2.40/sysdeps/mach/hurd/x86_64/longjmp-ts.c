/* Perform a `longjmp' on a Mach thread_state.  x86_64 version.
   Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

#include <hurd/signal.h>
#include <setjmp.h>
#include <jmpbuf-offsets.h>
#include <mach/thread_status.h>


/* Set up STATE to do the equivalent of `longjmp (ENV, VAL);'.  */

void
_hurd_longjmp_thread_state (void *state, jmp_buf env, int val)
{
  struct i386_thread_state *ts = state;

  ts->rbx = env[0].__jmpbuf[JB_RBX];
  ts->rbp = env[0].__jmpbuf[JB_RBP];
  ts->r12 = env[0].__jmpbuf[JB_R12];
  ts->r13 = env[0].__jmpbuf[JB_R13];
  ts->r14 = env[0].__jmpbuf[JB_R14];
  ts->r15 = env[0].__jmpbuf[JB_R15];
  ts->ursp = env[0].__jmpbuf[JB_RSP];
  ts->rip = env[0].__jmpbuf[JB_PC];
  ts->rax = val ?: 1;
}
