/* Copyright (C) 2006-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#define JB_X19            0
#define JB_X20            1
#define JB_X21            2
#define JB_X22            3
#define JB_X23            4
#define JB_X24            5
#define JB_X25            6
#define JB_X26            7
#define JB_X27            8
#define JB_X28            9
#define JB_X29           10
#define JB_LR            11
#define JB_SP		 13

#define JB_D8		 14
#define JB_D9		 15
#define JB_D10		 16
#define JB_D11		 17
#define JB_D12		 18
#define JB_D13		 19
#define JB_D14		 20
#define JB_D15		 21

/* The target specific part of jmp_buf has no space for expansion but
   the public jmp_buf ABI type has.  Unfortunately there is another type
   that is used with setjmp APIs and exposed by thread cancellation (in
   binaries built with -fno-exceptions) which complicates the situation.

  // Internal layout of the public jmp_buf type on AArch64.
  // This is passed to setjmp, longjmp, sigsetjmp, siglongjmp.
  struct
  {
    uint64_t jmpbuf[22];     // Target specific part.
    uint32_t mask_was_saved; // savemask bool used by sigsetjmp/siglongjmp.
    uint32_t pad;
    uint64_t saved_mask;     // sigset_t bits used on linux.
    uint64_t unused[15];     // sigset_t bits not used on linux.
  };

  // Internal layout of the public __pthread_unwind_buf_t type.
  // This is passed to sigsetjmp with !savemask and to the internal
  // __libc_longjmp (currently alias of longjmp on AArch64).
  struct
  {
    uint64_t jmpbuf[22];     // Must match jmp_buf.
    uint32_t mask_was_saved; // Must match jmp_buf, always 0.
    uint32_t pad;
    void *prev;              // List for unwinding.
    void *cleanup;           // Cleanup handlers.
    uint32_t canceltype;     // 1 bit cancellation type.
    uint32_t pad2;
    void *pad3;
  };

  Ideally only the target specific part of jmp_buf (A) is accessed by
  __setjmp and __longjmp.  But that is always embedded into one of the
  two types above so the bits that are unused in those types (B) may be
  reused for target specific purposes.  Setjmp can't distinguish between
  jmp_buf and __pthread_unwind_buf_t, but longjmp can: only an internal
  longjmp call uses the latter, so state that is not needed for cancel
  cleanups can go to fields (C).  If generic code is refactored then the
  usage of additional fields can be optimized (D).  And some fields are
  only accessible in the savedmask case (E).  Reusability of jmp_buf
  fields on AArch64 for target purposes:

  struct
  {
    uint64_t A[22];  //   0 .. 176
    uint32_t D;      // 176 .. 180
    uint32_t B;      // 180 .. 184
    uint64_t D;      // 184 .. 192
    uint64_t C;      // 192 .. 200
    uint32_t C;      // 200 .. 204
    uint32_t B;      // 204 .. 208
    uint64_t B;      // 208 .. 216
    uint64_t E[12];  // 216 .. 312
  }

  The B fields can be used with minimal glibc code changes.  We need a
  64 bit field for the Guarded Control Stack pointer (GCSPR_EL0) which
  can use a C field too as cancellation cleanup does not execute RET
  for a previous BL of the cancelled thread, but that would require a
  custom __libc_longjmp.  This layout can change in the future.  */
#define JB_GCSPR 208

#ifndef  __ASSEMBLER__
#include <setjmp.h>
#include <stdint.h>
#include <sysdep.h>
#include <pointer_guard.h>

static inline uintptr_t __attribute__ ((unused))
_jmpbuf_sp (__jmp_buf jmpbuf)
{
  uintptr_t sp = jmpbuf[JB_SP];
  PTR_DEMANGLE (sp);
  return sp;
}
#endif

/* Helper for generic ____longjmp_chk(). */
#define JB_FRAME_ADDRESS(buf) \
  ((void *) _jmpbuf_sp (buf))
