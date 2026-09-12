/* Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <sframe-read.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unwind.h>
#include <uw-sigframe.h>
#include <ldsodefs.h>

/* Some arches like s390x needs an offset to correct the value where
   SP is located in relation to CFA.  */
#ifndef SFRAME_SP_VAL_OFFSET
#define SFRAME_SP_VAL_OFFSET 0
#endif

static inline _Unwind_Ptr
read_stack_value (_Unwind_Ptr loc)
{
  _Unwind_Ptr value = *((_Unwind_Ptr *) loc);
  return value;
}

/* Helper to avoid PLT call in libc.  Fixes elf/check-localplt
   errors.  */

static int
_dl_find_object_helper (void *address, struct dl_find_object *result)
{
  return GLRO (dl_find_object) (address, result);
}

/* Backtrace the stack and collect the stacktrace given SFrame info.
   If successful, store the return addresses in RA_LST.  The SIZE
   argument specifies the maximum number of return addresses that can
   be stored in RA_LST and contains the number of the addresses
   collected.  */

int
__stacktrace_sframe (void **ra_lst, int count, frame *frame)
{
  _Unwind_Ptr sframe_vma, cfa, return_addr, ra_stack_loc, fp_stack_loc, pc,
    frame_ptr;
  int cfa_offset, fp_offset, ra_offset, i;
  sframe_frame_row_entry fred, *frep = &fred;

  if (!ra_lst || !count)
    return 0;

  for (i = 0; i < count; i++)
    {
      _Unwind_Reason_Code err;
      struct dl_find_object data;
      sframe_decoder_ctx decoder_context, *dctx = &decoder_context;

      /* Clean decoder context.  */
      memset (dctx, 0, sizeof (sframe_decoder_ctx));

      /* Load and set up the SFrame stack trace info for pc.  */
      if (_dl_find_object_helper ((void *) frame->pc, &data) < 0)
	/* Force fallback to DWARF stacktracer.  */
	return 0;

      sframe_vma = (_Unwind_Ptr) data.dlfo_sframe;
      if (!sframe_vma || !(data.dlfo_flags & DLFO_FLAG_SFRAME))
	{
#ifdef MD_DECODE_SIGNAL_FRAME
	  /* If there is no valid SFrame section or SFrame section is
	     corrupted then check if it is a signal frame.  */
	  if (MD_DECODE_SIGNAL_FRAME (frame) == _URC_NO_REASON)
	    {
	      ra_lst[i] = (void *) frame->pc;
	      continue;
	    }
#endif
	  /* Force fallback to DWARF stacktracer.  */
	  return 0;
	}

      /* Decode the specified SFrame buffer populate sframe's decoder
	 context.  */
      if (__sframe_decode (dctx, (char *) data.dlfo_sframe) != _URC_NO_REASON)
	/* Force fallback to DWARF stacktracer.  */
	return 0;

      pc = frame->pc - sframe_vma;
      /* Find the SFrame Row Entry which contains the PC.  */
      if (__sframe_find_fre (dctx, pc, frep) == _URC_END_OF_STACK)
	{
#ifdef MD_DECODE_SIGNAL_FRAME
	  /* If there are no valid FREs, check if it is a signal
	     frame, and if so decode it.  */
	  if (MD_DECODE_SIGNAL_FRAME (frame) == _URC_NO_REASON)
	    {
	      ra_lst[i] = (void *) frame->pc;
	      continue;
	    }
#endif
#ifdef MD_DETECT_OUTERMOST_FRAME
	  if (MD_DETECT_OUTERMOST_FRAME (frame) == _URC_END_OF_STACK)
	    return i;
#endif
	  /* Force fallback to DWARF stacktracer.  */
	  return 0;
	}

      /* Get the CFA offset from the FRE.  If offset is unavailable,
	 sets err.  */
      cfa_offset = __sframe_fre_get_cfa_offset (dctx, frep, &err);
      if (err != _URC_NO_REASON)
	/* Force fallback to DWARF stacktracer.  */
	return 0;

      /* Get CFA using base reg id from the FRE info.  */
      cfa = ((__sframe_fre_get_base_reg_id (frep)
	      == SFRAME_BASE_REG_SP) ? frame->sp : frame->fp) + cfa_offset;

      /* Get the RA offset from the FRE.  If the offset is
	 unavailable, sets err.  */
      ra_offset = __sframe_fre_get_ra_offset (dctx, frep, &err);
      if (err != _URC_NO_REASON)
	/* Force fallback to DWARF stacktracer.  */
	return 0;

      /* RA offset is available, get the value stored in the stack
	 location.  */
      ra_stack_loc = cfa + ra_offset;
      return_addr = read_stack_value (ra_stack_loc);

      ra_lst[i] = (void *) return_addr;

      /* Get the FP offset from the FRE.  If the offset is
	 unavailable, sets err.  */
      fp_offset = __sframe_fre_get_fp_offset (dctx, frep, &err);
      frame_ptr = frame->fp;
      if (err == _URC_NO_REASON)
	{
	  /* FP offset is available, get the value stored in the stack
	     location.  */
	  fp_stack_loc = cfa + fp_offset;
	  frame_ptr = read_stack_value (fp_stack_loc);
	}

      /* Set up for the next frame.  */
      frame->fp = frame_ptr;
      frame->sp = cfa + SFRAME_SP_VAL_OFFSET;
      frame->pc = return_addr;
    }
  return i;
}

libc_hidden_def (__stacktrace_sframe);

/* A noinline helper used to obtain the caller's current PC.  */

_Unwind_Ptr  __attribute__ ((noinline))
__getPC (void)
{
  return (_Unwind_Ptr)
    __builtin_extract_return_addr (__builtin_return_address (0));
}

libc_hidden_def (__getPC);

/* A noinline helper used to obtain the caller's current SP.  It
   mimics gcc14's __builtin_stack_address() functionality.  */

_Unwind_Ptr  __attribute__ ((noinline))
__getSP (void)
{
  return (_Unwind_Ptr) __builtin_dwarf_cfa() + SFRAME_SP_VAL_OFFSET;
}

libc_hidden_def (__getSP);
