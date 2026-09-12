/* Architecture specific code for pthread cancellation handling.
   Linux/PowerPC version.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#ifndef _NPTL_CANCELLATION_PC_CHECK
#define _NPTL_CANCELLATION_PC_CHECK

#include <sigcontextinfo.h>

/* For syscalls with side-effects (e.g read that might return partial read),
   the kernel cannot restart the syscall when interrupted by a signal, it must
   return from the call with whatever partial result.  In this case, the saved
   program counter is set just after the syscall instruction, so the SIGCANCEL
   handler should not act on cancellation.

   The __syscall_cancel_arch function, used for all cancellable syscalls,
   contains two extra markers, __syscall_cancel_arch_start and
   __syscall_cancel_arch_end.  The former points to just before the initial
   conditional branch that checks if the thread has received a cancellation
   request, while former points to the instruction after the one responsible
   to issue the syscall.

   The function check if the program counter (PC) from ucontext_t CTX is
   within the start and then end boundary from the __syscall_cancel_arch
   bridge.  Return TRUE if the PC is within the boundary, meaning the
   syscall does not have any side effects; or FALSE otherwise.  */

static __always_inline bool
cancellation_pc_check (void *ctx)
{
  /* Both are defined in syscall_cancel.S.  */
  extern const char __syscall_cancel_arch_start[1];
  extern const char __syscall_cancel_arch_end_sc[1];
#if defined(USE_PPC_SVC) && defined(__powerpc64__)
  extern const char __syscall_cancel_arch_end_svc[1];
#endif

  uintptr_t pc = sigcontext_get_pc (ctx);

  return pc >= (uintptr_t) __syscall_cancel_arch_start
#if defined(USE_PPC_SVC) && defined(__powerpc64__)
	 && THREAD_GET_HWCAP() & PPC_FEATURE2_SCV
	    ? pc < (uintptr_t) __syscall_cancel_arch_end_sc
	    : pc < (uintptr_t) __syscall_cancel_arch_end_svc;
#else
	 && pc < (uintptr_t) __syscall_cancel_arch_end_sc;
#endif
}

#endif
