/* Mach thread state definitions for machine-independent code.  i386 version.
   Copyright (C) 1994-2024 Free Software Foundation, Inc.
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

#ifndef _MACH_X86_THREAD_STATE_H
#define _MACH_X86_THREAD_STATE_H 1

#include <mach/machine/thread_status.h>
#include <libc-pointer-arith.h>

/* This lets the kernel define segments for a new thread.  */
#define MACHINE_NEW_THREAD_STATE_FLAVOR	i386_THREAD_STATE
/* This makes the kernel load our segments descriptors.  */
#define MACHINE_THREAD_STATE_FLAVOR	i386_REGS_SEGS_STATE
#define MACHINE_THREAD_STATE_COUNT	i386_THREAD_STATE_COUNT

#define machine_thread_state i386_thread_state

#ifdef __x86_64__
#define PC rip
#define SP ursp
#define SYSRETURN rax
#define MACHINE_THREAD_STATE_FIX_NEW(ts) do { \
	asm ("mov %%cs, %w0" : "=q" ((ts)->cs)); \
} while(0)
#else
#define PC eip
#define SP uesp
#define SYSRETURN eax
#define MACHINE_THREAD_STATE_FIX_NEW(ts) do { \
	asm ("mov %%cs, %w0" : "=q" ((ts)->cs)); \
	asm ("mov %%ds, %w0" : "=q" ((ts)->ds)); \
	asm ("mov %%es, %w0" : "=q" ((ts)->es)); \
	asm ("mov %%fs, %w0" : "=q" ((ts)->fs)); \
	asm ("mov %%gs, %w0" : "=q" ((ts)->gs)); \
} while(0)
#endif

struct machine_thread_all_state
  {
    int set;			/* Mask of bits (1 << FLAVOR).  */
    struct i386_thread_state basic;
    struct i386_float_state fpu;
  };

#ifdef __x86_64__
/* We're setting up the stack to perform a function call.  On function entry,
   the stack pointer must be 8 bytes less than 16-aligned.  */
#define PTR_ALIGN_DOWN_8_16(ptr)					      \
 ({ uintptr_t __ptr = PTR_ALIGN_DOWN (ptr, 8);				      \
    PTR_IS_ALIGNED (__ptr, 16) ? (__ptr - 8) : __ptr; })

#define MACHINE_THREAD_STATE_SETUP_CALL(ts, stack, size, func)		      \
  ((ts)->SP = PTR_ALIGN_DOWN_8_16 ((uintptr_t) (stack) + (size)),	      \
   (ts)->PC = (uintptr_t) func)
#endif

#include <sysdeps/mach/thread_state.h>

#endif /* mach/x86/thread_state.h */
