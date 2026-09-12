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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sframe-read.h>

/* Get the SFrame header size.  */

static inline uint32_t
sframe_get_hdr_size (sframe_header *sfh)
{
  return SFRAME_V1_HDR_SIZE (*sfh);
}

/* Access functions for frame row entry data.  */

static inline uint8_t
sframe_fre_get_offset_count (uint8_t fre_info)
{
  return SFRAME_V1_FRE_OFFSET_COUNT (fre_info);
}

static inline uint8_t
sframe_fre_get_offset_size (uint8_t fre_info)
{
  return SFRAME_V1_FRE_OFFSET_SIZE (fre_info);
}

static inline bool
sframe_get_fre_ra_mangled_p (uint8_t fre_info)
{
  return SFRAME_V1_FRE_MANGLED_RA_P (fre_info);
}

/* Access functions for info from function descriptor entry.  */

static uint32_t
sframe_get_fre_type (sframe_func_desc_entry *fdep)
{
  uint32_t fre_type = 0;
  if (fdep != NULL)
    fre_type = SFRAME_V1_FUNC_FRE_TYPE (fdep->sfde_func_info);
  return fre_type;
}

static uint32_t
sframe_get_fde_type (sframe_func_desc_entry *fdep)
{
  uint32_t fde_type = 0;
  if (fdep != NULL)
    fde_type = SFRAME_V1_FUNC_FDE_TYPE (fdep->sfde_func_info);
  return fde_type;
}

/* Check if SFrame header has valid data.  Only consider SFrame type
   2.  */

static bool
sframe_header_sanity_check_p (sframe_header *hp)
{
  /* Check preamble is valid.  */
  if ((hp->sfh_preamble.sfp_magic != SFRAME_MAGIC)
      || (hp->sfh_preamble.sfp_version != SFRAME_VERSION_2)
      || (hp->sfh_preamble.sfp_flags & ~SFRAME_V2_F_ALL_FLAGS))
    return false;

  /* Check offsets are valid.  */
  if (hp->sfh_fdeoff > hp->sfh_freoff)
    return false;

  return true;
}

/* Get the FRE start address size.  */

static size_t
sframe_fre_start_addr_size (uint32_t fre_type)
{
  size_t addr_size = 0;
  switch (fre_type)
    {
    case SFRAME_FRE_TYPE_ADDR1:
      addr_size = 1;
      break;
    case SFRAME_FRE_TYPE_ADDR2:
      addr_size = 2;
      break;
    case SFRAME_FRE_TYPE_ADDR4:
      addr_size = 4;
      break;
    default:
      break;
    }
  return addr_size;
}

/* Check if the FREP has valid data.  */

static bool
sframe_fre_sanity_check_p (sframe_frame_row_entry *frep)
{
  uint8_t offset_size, offset_cnt;
  uint8_t fre_info;

  if (frep == NULL)
    return false;

  fre_info = frep->fre_info;
  offset_size = sframe_fre_get_offset_size (fre_info);

  if (offset_size != SFRAME_FRE_OFFSET_1B
      && offset_size != SFRAME_FRE_OFFSET_2B
      && offset_size != SFRAME_FRE_OFFSET_4B)
    return false;

  offset_cnt = sframe_fre_get_offset_count (fre_info);
  if (offset_cnt > MAX_NUM_STACK_OFFSETS)
    return false;

  return true;
}

/* Get FRE_INFO's offset size in bytes.  */

static size_t
sframe_fre_offset_bytes_size (uint8_t fre_info)
{
  uint8_t offset_size, offset_cnt;

  offset_size = sframe_fre_get_offset_size (fre_info);

  offset_cnt = sframe_fre_get_offset_count (fre_info);

  if (offset_size == SFRAME_FRE_OFFSET_2B
      || offset_size == SFRAME_FRE_OFFSET_4B)	/* 2 or 4 bytes.  */
    return (offset_cnt * (offset_size * 2));

  return (offset_cnt);
}

/* Get total size in bytes to represent FREP in the binary format.  This
   includes the starting address, FRE info, and all the offsets.  */

static size_t
sframe_fre_entry_size (sframe_frame_row_entry *frep, size_t addr_size)
{
  if (frep == NULL)
    return 0;

  uint8_t fre_info = frep->fre_info;

  return (addr_size + sizeof (frep->fre_info)
	  + sframe_fre_offset_bytes_size (fre_info));
}

/* Get SFrame header from the given decoder context DCTX.  */

static inline sframe_header *
sframe_decoder_get_header (sframe_decoder_ctx *dctx)
{
  sframe_header *hp = NULL;
  if (dctx != NULL)
    hp = &dctx->sfd_header;
  return hp;
}

/* Get the offset of the sfde_func_start_address field (from the start of the
   on-disk layout of the SFrame section) of the FDE at FUNC_IDX in the decoder
   context DCTX.  */

static uint32_t
sframe_decoder_get_offsetof_fde_start_addr (sframe_decoder_ctx *dctx,
					    uint32_t func_idx,
					    _Unwind_Reason_Code *errp)
{
  sframe_header *dhp;

  dhp = sframe_decoder_get_header (dctx);
  if (dhp == NULL)
    {
      if (errp != NULL)
	*errp = _URC_END_OF_STACK;
      return 0;
    }

  if (func_idx >= dhp->sfh_num_fdes)
    {
      if (errp != NULL)
	*errp = _URC_END_OF_STACK;
      return 0;
    }
  else if (errp != NULL)
    *errp = _URC_NO_REASON;

  return (sframe_get_hdr_size (dhp)
	  + func_idx * sizeof (sframe_func_desc_entry)
	  + offsetof (sframe_func_desc_entry, sfde_func_start_address));
}


/* Get the offset of the start PC of the SFrame FDE at FUNC_IDX from
   the start of the SFrame section. If the flag
   SFRAME_F_FDE_FUNC_START_PCREL is set, sfde_func_start_address is
   the offset of the start PC of the function from the field itself.

   If FUNC_IDX is not a valid index in the given decoder object, returns 0.  */

static int32_t
sframe_decoder_get_secrel_func_start_addr (sframe_decoder_ctx *dctx,
					   uint32_t func_idx)
{
  int32_t func_start_addr;
  _Unwind_Reason_Code err = 0;
  int32_t offsetof_fde_in_sec = 0;

  /* Check if we have SFRAME_F_FDE_FUNC_START_PCREL.  */
  sframe_header *sh = &dctx->sfd_header;
  if ((sh->sfh_preamble.sfp_flags & SFRAME_F_FDE_FUNC_START_PCREL))
    {
      offsetof_fde_in_sec =
	sframe_decoder_get_offsetof_fde_start_addr (dctx, func_idx, &err);
      /* If func_idx is not a valid index, return 0.  */
      if (err == _URC_END_OF_STACK)
	return 0;
    }

  func_start_addr = dctx->sfd_funcdesc[func_idx].sfde_func_start_address;

  return func_start_addr + offsetof_fde_in_sec;
}

/* Check if the SFrame Frame Row Entry identified via the
   START_IP_OFFSET and the END_IP_OFFSET (for SFrame FDE at
   FUNC_IDX).  */

static bool
sframe_fre_check_range_p (sframe_decoder_ctx *dctx, uint32_t func_idx,
			  uint32_t start_ip_offset, uint32_t end_ip_offset,
			  int32_t pc)
{
  sframe_func_desc_entry *fdep;
  int32_t func_start_addr;
  uint8_t rep_block_size;
  uint32_t fde_type;
  uint32_t pc_offset;
  bool mask_p;

  fdep = &dctx->sfd_funcdesc[func_idx];
  if (fdep == NULL)
    return false;

  func_start_addr = sframe_decoder_get_secrel_func_start_addr (dctx, func_idx);
  fde_type = sframe_get_fde_type (fdep);
  mask_p = (fde_type == SFRAME_FDE_TYPE_PCMASK);
  rep_block_size = fdep->sfde_func_rep_size;

  if (func_start_addr > pc)
    return false;

  /* Given func_start_addr <= pc, pc - func_start_addr must be positive.  */
  pc_offset = pc - func_start_addr;
  /* For SFrame FDEs encoding information for repetitive pattern of insns,
     masking with the rep_block_size is necessary to find the matching FRE.  */
  if (mask_p)
    pc_offset = pc_offset % rep_block_size;

  return (start_ip_offset <= pc_offset) && (end_ip_offset >= pc_offset);
}

/* Get IDX'th offset from FRE.  Set ERRP as applicable.  */

static int32_t
sframe_get_fre_offset (sframe_frame_row_entry *fre,
		       int idx,
		       _Unwind_Reason_Code *errp)
{
  uint8_t offset_cnt, offset_size;

  if (!sframe_fre_sanity_check_p (fre))
    {
      *errp = _URC_END_OF_STACK;
      return 0;
    }

  offset_cnt = sframe_fre_get_offset_count (fre->fre_info);
  offset_size = sframe_fre_get_offset_size (fre->fre_info);

  if (offset_cnt < (idx + 1))
    {
      *errp = _URC_END_OF_STACK;
      return 0;
    }
  *errp = _URC_NO_REASON;

  if (offset_size == SFRAME_FRE_OFFSET_1B)
    {
      int8_t *sp = (int8_t *)fre->fre_offsets;
      return sp[idx];
    }
  else if (offset_size == SFRAME_FRE_OFFSET_2B)
    {
      int16_t *sp = (int16_t *)fre->fre_offsets;
      return sp[idx];
    }
  else
    {
      int32_t *ip = (int32_t *)fre->fre_offsets;
      return ip[idx];
    }
}

/* Decode the SFrame FRE start address offset value from FRE_BUF in on-disk
   binary format, given the FRE_TYPE.  Updates the FRE_START_ADDR.  */

static void
sframe_decode_fre_start_address (const char *fre_buf,
				 uint32_t *fre_start_addr,
				 uint32_t fre_type)
{
  uint32_t saddr = 0;

  if (fre_type == SFRAME_FRE_TYPE_ADDR1)
    {
      uint8_t *uc = (uint8_t *)fre_buf;
      saddr = (uint32_t)*uc;
    }
  else if (fre_type == SFRAME_FRE_TYPE_ADDR2)
    {
      uint16_t *ust = (uint16_t *)fre_buf;
      saddr = (uint32_t)*ust;
    }
  else if (fre_type == SFRAME_FRE_TYPE_ADDR4)
    {
      uint32_t *uit = (uint32_t *)fre_buf;
      saddr = (uint32_t)*uit;
    }
  else
    return;

  *fre_start_addr = saddr;
}

/* Find the function descriptor entry starting which contains the specified
   address ADDR.  */

static sframe_func_desc_entry *
sframe_get_funcdesc_with_addr_internal (sframe_decoder_ctx *ctx, int32_t addr,
					int *errp, uint32_t *func_idx)
{
  sframe_header *dhp;
  sframe_func_desc_entry *fdp;
  int low, high;

  if (ctx == NULL)
    return NULL;

  dhp = sframe_decoder_get_header (ctx);

  if (dhp == NULL || dhp->sfh_num_fdes == 0 || ctx->sfd_funcdesc == NULL)
    return NULL;
  /* If the FDE sub-section is not sorted on PCs, skip the lookup because
     binary search cannot be used.  */
  if ((dhp->sfh_preamble.sfp_flags & SFRAME_F_FDE_SORTED) == 0)
    return NULL;

  /* Do the binary search.  */
  fdp = (sframe_func_desc_entry *) ctx->sfd_funcdesc;
  low = 0;
  high = dhp->sfh_num_fdes - 1;
  while (low <= high)
    {
      int mid = low + (high - low) / 2;

      /* Given sfde_func_start_address <= addr,
	 addr - sfde_func_start_address must be positive.  */
      if (sframe_decoder_get_secrel_func_start_addr (ctx, mid) <= addr
	  && ((uint32_t)(addr - sframe_decoder_get_secrel_func_start_addr (ctx,
									   mid))
	      < fdp[mid].sfde_func_size))
	{
	  *func_idx = mid;
	  return fdp + mid;
	}

      if (sframe_decoder_get_secrel_func_start_addr (ctx, mid) < addr)
	low = mid + 1;
      else
	high = mid - 1;
    }

  return NULL;
}

/* Get the end IP offset for the FRE at index i in the FDEP.  The buffer FRES
   is the starting location for the FRE.  */

static uint32_t
sframe_fre_get_end_ip_offset (sframe_func_desc_entry *fdep, unsigned int i,
			      const char *fres)
{
  uint32_t end_ip_offset = 0;
  uint32_t fre_type;

  fre_type = sframe_get_fre_type (fdep);

  /* Get the start address of the next FRE in sequence.  */
  if (i < fdep->sfde_func_num_fres - 1)
    {
      sframe_decode_fre_start_address (fres, &end_ip_offset, fre_type);
      end_ip_offset -= 1;
    }
  else
    /* The end IP offset for the FRE needs to be deduced from the function
       size.  */
    end_ip_offset = fdep->sfde_func_size - 1;

  return end_ip_offset;
}

/* Get the SFrame's fixed FP offset given the decoder context CTX.  */

static int8_t
sframe_decoder_get_fixed_fp_offset (sframe_decoder_ctx *ctx)
{
  sframe_header *dhp;
  dhp = sframe_decoder_get_header (ctx);
  return dhp->sfh_cfa_fixed_fp_offset;
}

/* Get the SFrame's fixed RA offset given the decoder context CTX.  */

static int8_t
sframe_decoder_get_fixed_ra_offset (sframe_decoder_ctx *ctx)
{
  sframe_header *dhp;
  dhp = sframe_decoder_get_header (ctx);
  return dhp->sfh_cfa_fixed_ra_offset;
}

/* Get the base reg id from the FRE info.  Set errp if failure.  */

uint8_t
__sframe_fre_get_base_reg_id (sframe_frame_row_entry *fre)
{
  uint8_t fre_info = fre->fre_info;
  return SFRAME_V1_FRE_CFA_BASE_REG_ID (fre_info);
}

/* Get the CFA offset from the FRE.  If the offset is unavailable,
   sets errp.  */

int32_t
__sframe_fre_get_cfa_offset (sframe_decoder_ctx *dctx __attribute__ ((__unused__)),
			     sframe_frame_row_entry *fre,
			     _Unwind_Reason_Code *errp)
{
  return sframe_get_fre_offset (fre, SFRAME_FRE_CFA_OFFSET_IDX, errp);
}

/* Get the FP offset from the FRE.  If the offset is unavailable, sets
   errp.  */

int32_t
__sframe_fre_get_fp_offset (sframe_decoder_ctx *dctx,
			    sframe_frame_row_entry *fre,
			    _Unwind_Reason_Code *errp)
{
  uint32_t fp_offset_idx = 0;
  int8_t fp_offset = sframe_decoder_get_fixed_fp_offset (dctx);

  *errp = _URC_NO_REASON;
  /* If the FP offset is not being tracked, return the fixed FP offset
     from the SFrame header.  */
  if (fp_offset != SFRAME_CFA_FIXED_FP_INVALID)
    return fp_offset;

  /* In some ABIs, the stack offset to recover RA (using the CFA) from is
     fixed (like AMD64).  In such cases, the stack offset to recover FP will
     appear at the second index.  */
  fp_offset_idx = ((sframe_decoder_get_fixed_ra_offset (dctx)
		    != SFRAME_CFA_FIXED_RA_INVALID)
		   ? SFRAME_FRE_RA_OFFSET_IDX
		   : SFRAME_FRE_FP_OFFSET_IDX);
  return sframe_get_fre_offset (fre, fp_offset_idx, errp);
}

/* Get the RA offset from the FRE.  If the offset is unavailable, sets
   errp.  */

int32_t
__sframe_fre_get_ra_offset (sframe_decoder_ctx *dctx,
			    sframe_frame_row_entry *fre,
			    _Unwind_Reason_Code *errp)
{
  int8_t ra_offset = sframe_decoder_get_fixed_ra_offset (dctx);
  *errp = _URC_NO_REASON;

  /* If the RA offset was not being tracked, return the fixed RA offset
     from the SFrame header.  */
  if (ra_offset != SFRAME_CFA_FIXED_RA_INVALID)
    return ra_offset;

  /* Otherwise, get the RA offset from the FRE.  */
  return sframe_get_fre_offset (fre, SFRAME_FRE_RA_OFFSET_IDX, errp);
}

/* Decode the specified SFrame buffer SF_BUF and return the new SFrame
   decoder context.  */

_Unwind_Reason_Code
__sframe_decode (sframe_decoder_ctx *dctx, const char *sf_buf)
{
  const sframe_preamble *sfp;
  size_t hdrsz;
  sframe_header *sfheaderp;
  char *frame_buf;

  int fidx_size;
  uint32_t fre_bytes;

  if (sf_buf == NULL)
    return _URC_END_OF_STACK;

  sfp = (const sframe_preamble *) sf_buf;

  /* Check for foreign endianness.  */
  if (sfp->sfp_magic != SFRAME_MAGIC)
    return _URC_END_OF_STACK;

  frame_buf = (char *)sf_buf;

  /* Handle the SFrame header.  */
  dctx->sfd_header = *(sframe_header *) frame_buf;

  /* Validate the contents of SFrame header.  */
  sfheaderp = &dctx->sfd_header;
  if (!sframe_header_sanity_check_p (sfheaderp))
    return _URC_END_OF_STACK;

  hdrsz = sframe_get_hdr_size (sfheaderp);
  frame_buf += hdrsz;

  /* Handle the SFrame Function Descriptor Entry section.  */
  fidx_size
    = sfheaderp->sfh_num_fdes * sizeof (sframe_func_desc_entry);
  dctx->sfd_funcdesc = (sframe_func_desc_entry *)frame_buf;
  frame_buf += (fidx_size);

  dctx->sfd_fres = frame_buf;
  fre_bytes = sfheaderp->sfh_fre_len;
  dctx->sfd_fre_nbytes = fre_bytes;

  return _URC_NO_REASON;
}

/* Find the SFrame Row Entry which contains the PC.  Returns
   _URC_END_OF_STACK if failure.  */

_Unwind_Reason_Code
__sframe_find_fre (sframe_decoder_ctx *ctx, int32_t pc,
		   sframe_frame_row_entry *frep)
{
  sframe_func_desc_entry *fdep;
  uint32_t func_idx;
  uint32_t fre_type, i;
  uint32_t start_ip_offset;
  int32_t func_start_addr;
  uint32_t end_ip_offset;
  const char *fres;
  size_t size = 0;
  int err = 0;

  if ((ctx == NULL) || (frep == NULL))
    return _URC_END_OF_STACK;

  /* Find the FDE which contains the PC, then scan its fre entries.  */
  fdep = sframe_get_funcdesc_with_addr_internal (ctx, pc, &err, &func_idx);
  if (fdep == NULL || ctx->sfd_fres == NULL)
    return _URC_END_OF_STACK;

  fre_type = sframe_get_fre_type (fdep);

  fres = ctx->sfd_fres + fdep->sfde_func_start_fre_off;
  func_start_addr = sframe_decoder_get_secrel_func_start_addr (ctx, func_idx);

  for (i = 0; i < fdep->sfde_func_num_fres; i++)
    {
      size_t addr_size;

      /* Partially decode the FRE.  */
      sframe_decode_fre_start_address (fres, &frep->fre_start_addr, fre_type);

      addr_size = sframe_fre_start_addr_size (fre_type);
      if (addr_size == 0)
	return _URC_END_OF_STACK;

      frep->fre_info = *(uint8_t *)(fres + addr_size);
      size = sframe_fre_entry_size (frep, addr_size);

      start_ip_offset = frep->fre_start_addr;
      end_ip_offset = sframe_fre_get_end_ip_offset (fdep, i, fres + size);

      /* Stop search if FRE's start_ip is greater than pc.  Given
	func_start_addr <= pc, pc - func_start_addr must be positive.  */
      if (start_ip_offset > (uint32_t) (pc - func_start_addr))
	return _URC_END_OF_STACK;

      if (sframe_fre_check_range_p (ctx, func_idx, start_ip_offset,
				    end_ip_offset, pc))
	{
	  /* Decode last FRE bits: offsets size.  */
	  frep->fre_offsets = fres + addr_size + sizeof (frep->fre_info);
	  return _URC_NO_REASON;
	}

      fres += size;
    }
  return _URC_END_OF_STACK;
}
