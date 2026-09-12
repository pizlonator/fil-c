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

#ifndef _SFRAME_API_H
#define _SFRAME_API_H

#include <sframe.h>
#include <stdbool.h>
#include <unwind.h>

#ifdef	__cplusplus
extern "C"
{
#endif

typedef struct sframe_decoder_ctx
{

  sframe_header sfd_header;

  sframe_func_desc_entry *sfd_funcdesc;
  /* SFrame FRE table.  */
  char *sfd_fres;
  /* Number of bytes needed for SFrame FREs.  */
  int sfd_fre_nbytes;
} sframe_decoder_ctx;

#define MAX_NUM_STACK_OFFSETS	3

/* User interfacing SFrame Row Entry.
   An abstraction provided by libsframe so the consumer is decoupled from
   the binary format representation of the same.

   The members are best ordered such that they are aligned at their natural
   boundaries.  This helps avoid usage of undesirable misaligned memory
   accesses.  See PR libsframe/29856.  */

typedef struct sframe_frame_row_entry
{
  uint32_t fre_start_addr;
  const char *fre_offsets;
  unsigned char fre_info;
} sframe_frame_row_entry;

/* The SFrame Decoder.  */

/* Decode the specified SFrame buffer CF_BUF and return the new SFrame
   decoder context.  */

extern _Unwind_Reason_Code
__sframe_decode (sframe_decoder_ctx *dctx, const char *cf_buf);

/* Find the SFrame Frame Row Entry which contains the PC.  Returns
   _URC_END_OF_STACK if failure.  */

extern _Unwind_Reason_Code
__sframe_find_fre (sframe_decoder_ctx *ctx, int32_t pc,
		   sframe_frame_row_entry *frep);

/* Get the base reg id from the FRE info.  */

extern uint8_t
__sframe_fre_get_base_reg_id (sframe_frame_row_entry *fre);

/* Get the CFA offset from the FRE.  Sets ERRP if an error is
   detected.  */

extern int32_t
__sframe_fre_get_cfa_offset (sframe_decoder_ctx *dtcx,
			     sframe_frame_row_entry *fre,
			     _Unwind_Reason_Code *errp);

/* Get the FP offset from the FRE.  If the offset is unavailable, sets
   ERRP.  */

extern int32_t
__sframe_fre_get_fp_offset (sframe_decoder_ctx *dctx,
			    sframe_frame_row_entry *fre,
			    _Unwind_Reason_Code *errp);

/* Get the RA offset from the FRE.  Sets ERRP if ra offset is
   unavailable.  */

extern int32_t
__sframe_fre_get_ra_offset (sframe_decoder_ctx *dctx,
			    sframe_frame_row_entry *fre,
			    _Unwind_Reason_Code *errp);

/* Get the offset of the sfde_func_start_address field.  */

extern uint32_t
__sframe_decoder_get_offsetof_fde_start_addr (sframe_decoder_ctx *dctx,
					      uint32_t func_idx,
					      _Unwind_Reason_Code *errp);
#ifdef	__cplusplus
}
#endif

#endif /* _SFRAME_API_H */
