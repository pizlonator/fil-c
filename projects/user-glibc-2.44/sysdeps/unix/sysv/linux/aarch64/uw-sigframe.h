/* Signal frame backtracing support for SFrame on AARCH64.
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
   implementation.  See libgcc/config/aarch64/linux-unwind.h  */

#include <signal.h>
#include <sys/ucontext.h>
#include <kernel_rt_sigframe.h>

#ifdef __AARCH64EL__
#define MOVZ_X8_8B      0xd2801168
#define SVC_0           0xd4000001
#else
#define MOVZ_X8_8B      0x681180d2
#define SVC_0           0x010000d4
#endif

#define MD_DECODE_SIGNAL_FRAME aarch64_decode_signal_frame

static _Unwind_Reason_Code
aarch64_decode_signal_frame (frame *frame)
{
  unsigned int *pc = (unsigned int *) frame->pc;
  mcontext_t *mt;
  struct kernel_rt_sigframe *rt_;

  if ((frame->pc & 3) != 0)
    return _URC_END_OF_STACK;

  /* A signal frame will have a return address pointing to
     __kernel_rt_sigreturn.  This code is hardwired as:

     0xd2801168         movz x8, #0x8b
     0xd4000001         svc  0x0
   */
  if (pc[0] != MOVZ_X8_8B || pc[1] != SVC_0)
    return _URC_END_OF_STACK;

  rt_ = (struct kernel_rt_sigframe *) frame->sp;
  mt = &rt_->uc.uc_mcontext;

  /* Frame pointer register number.  */
#define FP_REGNUM 30

  frame->pc = (_Unwind_Ptr) mt->pc;
  frame->sp = (_Unwind_Ptr) mt->sp;
  frame->fp = (_Unwind_Ptr) mt->regs[FP_REGNUM];
  return _URC_NO_REASON;
}

#define MD_DETECT_OUTERMOST_FRAME aarch64_detect_outermost_frame

static _Unwind_Reason_Code
aarch64_detect_outermost_frame (frame *frame)
{
  /* Initial frame has LR and FP set to zero.  We track only FP.  */
  if (frame->fp == 0)
    return _URC_END_OF_STACK;

  return _URC_NO_REASON;
}
