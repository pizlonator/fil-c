/* Signal frame backtracing support for SFrame on AMD, x86-64 and x86.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* This code is inspired from libgcc's MD_FALLBACK_FRAME_STATE_FOR
   implementation.  See libgcc/config/i386/linux-unwind.h  */

#include <signal.h>
#include <sys/ucontext.h>

#ifdef __x86_64__

/* SFrame is only supported by x86_64 targets.  */

#define MD_DECODE_SIGNAL_FRAME x86_64_decode_signal_frame

#ifdef __LP64__
#define RT_SIGRETURN_SYSCALL	0x050f0000000fc0c7ULL
#else
#define RT_SIGRETURN_SYSCALL	0x050f40000201c0c7ULL
#endif

static _Unwind_Reason_Code
x86_64_decode_signal_frame (frame *frame)
{
  unsigned char *pc = (unsigned char *) frame->pc;
  mcontext_t *st;

  unsigned char pc0 = *(unsigned char *)(pc + 0);
  unsigned long long pc1;
  memcpy (&pc1, pc + 1, sizeof (unsigned long long));

  /* movq $__NR_rt_sigreturn, %rax ; syscall.  */
  if ( pc0 == 0x48
      && pc1 == RT_SIGRETURN_SYSCALL)
    {
      ucontext_t *uc_ = (ucontext_t *)frame->sp;
      st = &uc_->uc_mcontext;
    }
  else
    return _URC_END_OF_STACK;

  frame->pc = (_Unwind_Ptr) st->gregs[REG_RIP];
  frame->sp = (_Unwind_Ptr) st->gregs[REG_RSP];
  frame->fp = (_Unwind_Ptr) st->gregs[REG_RBP];
  return _URC_NO_REASON;
}

#define MD_DETECT_OUTERMOST_FRAME x86_64_detect_outermost_frame

static _Unwind_Reason_Code
x86_64_detect_outermost_frame (frame *frame)
{
  /* Outermost frame has the frame pointer cleared.  */
  if (frame->fp == 0)
    return _URC_END_OF_STACK;

  return _URC_NO_REASON;
}

#endif /* ifdef __x86_64__  */
