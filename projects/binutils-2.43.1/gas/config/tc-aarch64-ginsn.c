/* tc-aarch64-ginsn.c -- Ginsn generation for the AArch64 ISA

   Copyright (C) 2024 Free Software Foundation, Inc.

   This file is part of GAS.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the license, or
   (at your option) any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING3. If not,
   see <http://www.gnu.org/licenses/>.  */

/* This file contains the implementation of the ginsn creation for aarch64
   instructions.  Most functions will read the aarch64_instruction inst
   object, but none should need to modify it.  */

#ifdef OBJ_ELF

/* Invalid DWARF register number.  Used when WZR / XZR is seen.  */
#define GINSN_DW2_REGNUM_INVALID  (~0U)

/* Return whether the given register number is a callee-saved register for
   SCFI purposes.

   Apart from the callee-saved GPRs, SCFI always tracks SP, FP and LR
   additionally.  As for the FP/Advanced SIMD registers, v8-v15 are
   callee-saved.  */

bool
aarch64_scfi_callee_saved_p (unsigned int dw2reg_num)
{
  /* PS: Ensure SCFI_MAX_REG_ID is the max DWARF register number to cover
     all the registers here.  */
  if (dw2reg_num == REG_SP /* x31.  */
      || dw2reg_num == REG_FP /* x29.  */
      || dw2reg_num == REG_LR /* x30.  */
      || (dw2reg_num >= 19 && dw2reg_num <= 28) /* x19 - x28.  */
      || (dw2reg_num >= 72 && dw2reg_num <= 79) /* v8 - v15.  */)
    return true;

  return false;
}

/* Get the DWARF register number for the given OPND.  If OPND is an address,
   the returned register is the base register.  If OPND spans multiple
   registers, the returned register is the first of those registers.  */

static unsigned int
ginsn_dw2_regnum (aarch64_opnd_info *opnd)
{
  enum aarch64_operand_class opnd_class;
  unsigned int dw2reg_num = 0;

  opnd_class = aarch64_get_operand_class (opnd->type);

  switch (opnd_class)
    {
    case AARCH64_OPND_CLASS_FP_REG:
      dw2reg_num = opnd->reg.regno + 64;
      break;
    case AARCH64_OPND_CLASS_SVE_REGLIST:
      dw2reg_num = opnd->reglist.first_regno + 64;
      break;
    case AARCH64_OPND_CLASS_ADDRESS:
      dw2reg_num = opnd->addr.base_regno;
      break;
    case AARCH64_OPND_CLASS_INT_REG:
    case AARCH64_OPND_CLASS_MODIFIED_REG:
      /* Use an invalid DWARF register value in case of WZR, else this will be an
	 incorrect dependency on REG_SP.  */
      if (aarch64_zero_register_p (opnd))
	dw2reg_num = GINSN_DW2_REGNUM_INVALID;
      else
	/* For GPRs of our interest (callee-saved regs, SP, FP, LR),
	   DWARF register number is the same as AArch64 register number.  */
	dw2reg_num = opnd->reg.regno;
      break;
    default:
      as_bad ("Unexpected operand class in ginsn_dw2_regnum");
      break;
    }

  return dw2reg_num;
}

static bool
ginsn_dw2_regnum_invalid_p (unsigned int opnd_reg)
{
  return (opnd_reg == GINSN_DW2_REGNUM_INVALID);
}

/* Generate ginsn for addsub instructions with immediate opnd.  */

static ginsnS *
aarch64_ginsn_addsub_imm (const symbolS *insn_end_sym)
{
  ginsnS *ginsn = NULL;
  bool add_p, sub_p;
  offsetT src_imm = 0;
  unsigned int dst_reg, opnd_reg;
  aarch64_opnd_info *dst, *opnd;
  ginsnS *(*ginsn_func) (const symbolS *, bool,
			 enum ginsn_src_type, unsigned int, offsetT,
			 enum ginsn_src_type, unsigned int, offsetT,
			 enum ginsn_dst_type, unsigned int, offsetT);

  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  add_p = aarch64_opcode_subclass_p (opcode, F_ARITH_ADD);
  sub_p = aarch64_opcode_subclass_p (opcode, F_ARITH_SUB);
  gas_assert (add_p || sub_p);
  ginsn_func = add_p ? ginsn_new_add : ginsn_new_sub;

  gas_assert (aarch64_num_of_operands (opcode) == 3);
  dst = &base->operands[0];
  opnd = &base->operands[1];

  if (aarch64_gas_internal_fixup_p () && inst.reloc.exp.X_op == O_constant)
    src_imm = inst.reloc.exp.X_add_number;
  /* For any other relocation type, e.g., in add reg, reg, symbol, skip now
     and handle via aarch64_ginsn_unhandled () code path.  */
  else if (inst.reloc.type != BFD_RELOC_UNUSED)
    return ginsn;
  /* FIXME - verify the understanding and remove assert.  */
  else
    gas_assert (0);

  dst_reg = ginsn_dw2_regnum (dst);
  opnd_reg = ginsn_dw2_regnum (opnd);

  if (ginsn_dw2_regnum_invalid_p (dst_reg)
      || ginsn_dw2_regnum_invalid_p (opnd_reg))
    return ginsn;

  ginsn = ginsn_func (insn_end_sym, true,
		      GINSN_SRC_REG, opnd_reg, 0,
		      GINSN_SRC_IMM, 0, src_imm,
		      GINSN_DST_REG, dst_reg, 0);
  ginsn_set_where (ginsn);

  return ginsn;
}

/* Generate ginsn for addsub instructions with reg opnd.  */

static ginsnS *
aarch64_ginsn_addsub_reg (const symbolS *insn_end_sym)
{
  ginsnS *ginsn = NULL;
  bool add_p, sub_p;
  unsigned int dst_reg, src1_reg, src2_reg;
  aarch64_opnd_info *dst, *src1, *src2;
  ginsnS *(*ginsn_func) (const symbolS *, bool,
			 enum ginsn_src_type, unsigned int, offsetT,
			 enum ginsn_src_type, unsigned int, offsetT,
			 enum ginsn_dst_type, unsigned int, offsetT);

  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  add_p = aarch64_opcode_subclass_p (opcode, F_ARITH_ADD);
  sub_p = aarch64_opcode_subclass_p (opcode, F_ARITH_SUB);
  gas_assert (add_p || sub_p);
  ginsn_func = add_p ? ginsn_new_add : ginsn_new_sub;

  gas_assert (aarch64_num_of_operands (opcode) == 3);
  dst = &base->operands[0];
  src1 = &base->operands[1];
  src2 = &base->operands[2];

  dst_reg = ginsn_dw2_regnum (dst);
  src1_reg = ginsn_dw2_regnum (src1);
  src2_reg = ginsn_dw2_regnum (src2);

  if (ginsn_dw2_regnum_invalid_p (dst_reg)
      || ginsn_dw2_regnum_invalid_p (src1_reg)
      || ginsn_dw2_regnum_invalid_p (src2_reg))
    return ginsn;

  /* ATM, shift amount, if any, cannot be represented in the GINSN_TYPE_ADD or
     GINSN_TYPE_SUB.  As the extra information does not impact SCFI
     correctness, skip generating ginsn for these cases.  Note
     TBD_GINSN_INFO_LOSS.  */
  if (src2->shifter.kind != AARCH64_MOD_NONE
      && (src2->shifter.kind != AARCH64_MOD_LSL || src2->shifter.amount != 0))
    return ginsn;

  ginsn = ginsn_func (insn_end_sym, true,
		      GINSN_SRC_REG, src1_reg, 0,
		      GINSN_SRC_REG, src2_reg, 0,
		      GINSN_DST_REG, dst_reg, 0);
  ginsn_set_where (ginsn);

  return ginsn;
}

/* Generate ginsn for the load pair and store pair instructions.  */

static ginsnS *
aarch64_ginsn_ldstp (const symbolS *insn_end_sym)
{
  ginsnS *ginsn = NULL;
  ginsnS *ginsn_ind = NULL;
  ginsnS *ginsn_mem1 = NULL;
  ginsnS *ginsn_mem2 = NULL;
  ginsnS *ginsn_mem = NULL;
  unsigned int opnd1_reg, opnd2_reg, addr_reg;
  offsetT offset, mem_offset;
  unsigned int width = 8;
  bool load_p = false;
  bool store_p = false;
  bool other_p = false;

  aarch64_opnd_info *opnd1, *opnd2, *addr;
  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  /* This function is for handling ldp / stp ops only.  */
  gas_assert (opcode->iclass == ldstpair_indexed
	      || opcode->iclass == ldstnapair_offs
	      || opcode->iclass == ldstpair_off);
  gas_assert (aarch64_num_of_operands (opcode) == 3);

  opnd1 = &base->operands[0];
  opnd2 = &base->operands[1];
  addr = &base->operands[2];

  load_p = aarch64_opcode_subclass_p (opcode, F_LDST_LOAD);
  store_p = aarch64_opcode_subclass_p (opcode, F_LDST_STORE);
  other_p = aarch64_opcode_subclass_p (opcode, F_SUBCLASS_OTHER);
  gas_assert (load_p || store_p || other_p);

  addr_reg = ginsn_dw2_regnum (addr);
  gas_assert (!addr->addr.offset.is_reg);
  mem_offset = addr->addr.offset.imm;

  offset = mem_offset;
  /* Handle address calculation.  */
  if ((addr->addr.preind || addr->addr.postind) && addr->addr.writeback)
    {
      /* Pre-indexed store, e.g., stp x29, x30, [sp, -128]!
	 Pre-indexed addressing is like offset addressing, except that
	 the base pointer is updated as a result of the instruction.

	 Post-indexed store, e.g., stp     x29, x30, [sp],128
	 Post-index addressing is useful for popping off the stack.  The
	 instruction loads the value from the location pointed at by the stack
	 pointer, and then moves the stack pointer on to the next full location
	 in the stack.  */
      ginsn_ind = ginsn_new_add (insn_end_sym, false,
				 GINSN_SRC_REG, addr_reg, 0,
				 GINSN_SRC_IMM, 0, mem_offset,
				 GINSN_DST_REG, addr_reg, 0);
      ginsn_set_where (ginsn_ind);

      /* With post-index addressing, the value is loaded from the address in
	 the base pointer, and then the pointer is updated.  With pre-index
	 addressing, the addr computation has already been explicitly done.  */
      offset = 0;
    }

  /* Insns like ldpsw (marked with subclass F_SUBCLASS_OTHER) do not need to
     generate any load or store for SCFI purposes.  Next, enforce that for CFI
     purposes, the width of save / restore operation has to be 8 bytes or more.
     However, the address processing component may have updated the stack
     pointer.  At least, emit that ginsn and return.  Also note,
     TBD_GINSN_GEN_NOT_SCFI.  */
  if (other_p || aarch64_get_qualifier_esize (opnd1->qualifier) < 8)
    return ginsn_ind;

  opnd1_reg = ginsn_dw2_regnum (opnd1);
  opnd2_reg = ginsn_dw2_regnum (opnd2);
  /* Save / restore of WZR is not of interest for SCFI.  Exit now if both
     registers are not of interest.  */
  if (ginsn_dw2_regnum_invalid_p (opnd1_reg)
      && ginsn_dw2_regnum_invalid_p (opnd2_reg))
    return ginsn_ind;

  if (opnd1->qualifier == AARCH64_OPND_QLF_S_Q)
    {
      width = 16;
      if (target_big_endian)
	offset += 8;
    }

  /* Load store pair where only one of the opnd registers is a zero register
     is possible.  E.g., stp	xzr, x19, [sp, #16].  */
  if (!ginsn_dw2_regnum_invalid_p (opnd1_reg))
    {
      if (store_p)
	{
	  ginsn_mem1 = ginsn_new_store (insn_end_sym, false,
					GINSN_SRC_REG, opnd1_reg,
					GINSN_DST_INDIRECT, addr_reg, offset);
	  ginsn_set_where (ginsn_mem1);
	}
      else
	{
	  ginsn_mem1 = ginsn_new_load (insn_end_sym, false,
				       GINSN_SRC_INDIRECT, addr_reg, offset,
				       GINSN_DST_REG, opnd1_reg);
	  ginsn_set_where (ginsn_mem1);
	}
      /* Keep track of the last memory ginsn created so far.  */
      ginsn_mem = ginsn_mem1;
    }
  if (!ginsn_dw2_regnum_invalid_p (opnd2_reg))
    {
      if (store_p)
	{
	  ginsn_mem2 = ginsn_new_store (insn_end_sym, false,
					GINSN_SRC_REG, opnd2_reg,
					GINSN_DST_INDIRECT, addr_reg,
					offset + width);
	  ginsn_set_where (ginsn_mem2);
	}
      else
	{
	  ginsn_mem2 = ginsn_new_load (insn_end_sym, false,
				       GINSN_SRC_INDIRECT, addr_reg, offset + width,
				       GINSN_DST_REG, opnd2_reg);
	  ginsn_set_where (ginsn_mem2);
	}
      /* Keep track of the last memory ginsn created so far.  */
      ginsn_mem = ginsn_mem2;
    }

  if (!ginsn_dw2_regnum_invalid_p (opnd1_reg)
      && !ginsn_dw2_regnum_invalid_p (opnd2_reg))
    goto link_two_ginsn_mem;
  else
    goto link_one_ginsn_mem;

link_one_ginsn_mem:
  /* Link the list of ginsns created.  */
  if (addr->addr.preind && addr->addr.writeback)
    gas_assert (!ginsn_link_next (ginsn_ind, ginsn_mem));

  if (addr->addr.postind && addr->addr.writeback)
    gas_assert (!ginsn_link_next (ginsn_mem, ginsn_ind));

  /* Make note of the first instruction in the list.  */
  ginsn = (addr->addr.preind && addr->addr.writeback) ? ginsn_ind : ginsn_mem;
  return ginsn;

link_two_ginsn_mem:
  /* Link the list of ginsns created.  */
  if (addr->addr.preind && addr->addr.writeback)
    gas_assert (!ginsn_link_next (ginsn_ind, ginsn_mem1));

  gas_assert (ginsn_mem1 && ginsn_mem2 && ginsn_mem1 != ginsn_mem2);
  gas_assert (!ginsn_link_next (ginsn_mem1, ginsn_mem2));

  if (addr->addr.postind && addr->addr.writeback)
    gas_assert (!ginsn_link_next (ginsn_mem2, ginsn_ind));

  /* Make note of the first instruction in the list.  */
  ginsn = (addr->addr.preind && addr->addr.writeback) ? ginsn_ind : ginsn_mem1;
  return ginsn;
}

/* Generate ginsn for load and store instructions.  */

static ginsnS *
aarch64_ginsn_ldstr (const symbolS *insn_end_sym)
{
  ginsnS *ginsn = NULL;
  ginsnS *ginsn_ind = NULL;
  ginsnS *ginsn_mem = NULL;
  unsigned int opnd_reg, addr_reg;
  offsetT offset, mem_offset;
  bool load_p = false;
  bool store_p = false;
  bool other_p = false;

  aarch64_opnd_info *opnd1, *addr;
  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  /* This function is for handling ldr, str ops only.  */
  gas_assert (opcode->iclass == ldst_imm9 || opcode->iclass == ldst_pos);
  gas_assert (aarch64_num_of_operands (opcode) == 2);

  opnd1 = &base->operands[0];
  addr = &base->operands[1];

  load_p = aarch64_opcode_subclass_p (opcode, F_LDST_LOAD);
  store_p = aarch64_opcode_subclass_p (opcode, F_LDST_STORE);
  other_p = aarch64_opcode_subclass_p (opcode, F_SUBCLASS_OTHER);
  gas_assert (load_p || store_p || other_p);

  addr_reg = ginsn_dw2_regnum (addr);

  if (aarch64_gas_internal_fixup_p () && inst.reloc.exp.X_op == O_constant)
    mem_offset = inst.reloc.exp.X_add_number;
  else
    {
      gas_assert (!addr->addr.offset.is_reg);
      mem_offset = addr->addr.offset.imm;
    }

  offset = mem_offset;
  /* Handle address calculation.  */
  if ((addr->addr.preind || addr->addr.postind) && addr->addr.writeback)
    {
      ginsn_ind = ginsn_new_add (insn_end_sym, false,
				 GINSN_SRC_REG, addr_reg, 0,
				 GINSN_SRC_IMM, 0, mem_offset,
				 GINSN_DST_REG, addr_reg, 0);
      ginsn_set_where (ginsn_ind);

      /* With post-index addressing, the value is loaded from the address in
	 the base pointer, and then the pointer is updated.  With pre-index
	 addressing, the addr computation has already been explicitly done.  */
      offset = 0;
    }

  /* Insns like stg, prfm, ldrsw etc. (marked with subclass F_SUBCLASS_OTHER)
     do not need to generate any load / store ginsns for SCFI purposes.  Next,
     enforce that for CFI purposes, the width of save / restore operation has
     to be 8 bytes or more.  That said, the address processing component may
     have updated the stack pointer.  At least, emit that ginsn and return.
     Also note, TBD_GINSN_GEN_NOT_SCFI.  */
  if (other_p || aarch64_get_qualifier_esize (opnd1->qualifier) < 8)
    return ginsn_ind;

  opnd_reg = ginsn_dw2_regnum (opnd1);
  /* Save / restore of WZR is not of interest for SCFI.  */
  if (ginsn_dw2_regnum_invalid_p (opnd_reg))
    return ginsn_ind;

  if (target_big_endian && opnd1->qualifier == AARCH64_OPND_QLF_S_Q)
    offset += 8;

  if (store_p)
    ginsn_mem = ginsn_new_store (insn_end_sym, false,
				 GINSN_SRC_REG, opnd_reg,
				 GINSN_DST_INDIRECT, addr_reg, offset);
  else
    ginsn_mem = ginsn_new_load (insn_end_sym, false,
				GINSN_SRC_INDIRECT, addr_reg, offset,
				GINSN_DST_REG, opnd_reg);
  ginsn_set_where (ginsn_mem);

  if (addr->addr.preind && addr->addr.writeback)
    gas_assert (!ginsn_link_next (ginsn_ind, ginsn_mem));
  else if (addr->addr.postind && addr->addr.writeback)
    gas_assert (!ginsn_link_next (ginsn_mem, ginsn_ind));

  /* Make note of the first instruction in the list.  */
  ginsn = (addr->addr.preind && addr->addr.writeback) ? ginsn_ind : ginsn_mem;

  return ginsn;
}

/* Generate ginsn for unconditional branch instructions.  */

static ginsnS *
aarch64_ginsn_branch_uncond (const symbolS *insn_end_sym)
{
  ginsnS *ginsn = NULL;
  const symbolS *src_symbol = NULL;
  enum ginsn_src_type src_type = GINSN_SRC_UNKNOWN;
  unsigned int src_reg = 0;

  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  if (opcode->iclass == branch_imm
      && (inst.reloc.type == BFD_RELOC_AARCH64_CALL26
	  || inst.reloc.type == BFD_RELOC_AARCH64_JUMP26))
    {
      if (inst.reloc.exp.X_add_number)
	{
	  /* A non-zero addend in b/bl target makes control-flow tracking
	     difficult.  Skip SCFI for now.  */
	  as_bad (_("SCFI: %#x op with non-zero addend to sym not supported"),
		  opcode->opcode);
	  return ginsn;
	}
      /* b or bl.  */
      src_symbol = inst.reloc.exp.X_add_symbol;
      src_type = GINSN_SRC_SYMBOL;
    }
  else if (opcode->iclass == branch_reg
	   && aarch64_num_of_operands (opcode) >= 1)
    {
      /* Some insns (e.g., braa, blraa etc.) may have > 1 operands.  For
	 current SCFI implementation, it suffices however to simply pass
	 the information about the first source.  Although, strictly speaking,
	 (if reg) the source info is currently of no material use either.  */
      src_type = GINSN_SRC_REG;
      src_reg = ginsn_dw2_regnum (&base->operands[0]);
    }
  else
    /* Skip insns like branch imm.  */
    return ginsn;

  if (aarch64_opcode_subclass_p (opcode, F_BRANCH_CALL))
    {
      gas_assert (src_type != GINSN_SRC_UNKNOWN);
      ginsn = ginsn_new_call (insn_end_sym, true,
			      src_type, src_reg, src_symbol);
    }
  else if (aarch64_opcode_subclass_p (opcode, F_BRANCH_RET))
    /* TBD_GINSN_REPRESENTATION_LIMIT.  The following function to create a
       GINSN_TYPE_RETURN does not allow src info ATM.  */
    ginsn = ginsn_new_return (insn_end_sym, true);
  else
    ginsn = ginsn_new_jump (insn_end_sym, true,
			    src_type, src_reg, src_symbol);

  ginsn_set_where (ginsn);

  return ginsn;
}

/* Generate ginsn for conditional branch instructions.  */

static ginsnS *
aarch64_ginsn_branch_cond (const symbolS *insn_end_sym)
{
  ginsnS *ginsn = NULL;
  const symbolS *src_symbol;
  enum ginsn_src_type src_type;

  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  if (inst.reloc.type == BFD_RELOC_AARCH64_BRANCH19
      || inst.reloc.type == BFD_RELOC_AARCH64_TSTBR14)
    {
      if (inst.reloc.exp.X_add_number)
	{
	  /* A non-zero addend in target makes control-flow tracking
	     difficult.  Skip SCFI for now.  */
	  as_bad (_("SCFI: %#x op with non-zero addend to sym not supported"),
		  opcode->opcode);
	  return ginsn;
	}

      src_symbol = inst.reloc.exp.X_add_symbol;
      src_type = GINSN_SRC_SYMBOL;

      ginsn = ginsn_new_jump_cond (insn_end_sym, true, src_type, 0, src_symbol);
      ginsn_set_where (ginsn);
    }

  return ginsn;
}

/* Generate ginsn for mov instructions with reg opnd.  */

static ginsnS *
aarch64_ginsn_mov_reg (const symbolS *insn_end_sym)
{
  ginsnS *ginsn = NULL;
  unsigned int src_reg = 0, dst_reg;
  aarch64_opnd_info *src, *dst;
  offsetT src_imm = 0;
  enum ginsn_src_type src_type;

  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  gas_assert (aarch64_num_of_operands (opcode) == 2);

  dst = &base->operands[0];
  src = &base->operands[1];

  dst_reg = ginsn_dw2_regnum (dst);
  src_reg = ginsn_dw2_regnum (src);
  src_type = GINSN_SRC_REG;

  /* FIXME Explicitly bar GINSN_TYPE_MOV with a GINSN_DW2_REGNUM_INVALID in src
     or dest at this time.  This can be removed later when SCFI machinery is
     more robust to deal with GINSN_DW2_REGNUM_INVALID.  */
  if (ginsn_dw2_regnum_invalid_p (dst_reg)
      || ginsn_dw2_regnum_invalid_p (src_reg))
    return ginsn;

  ginsn = ginsn_new_mov (insn_end_sym, false,
			 src_type, src_reg, src_imm,
			 GINSN_DST_REG, dst_reg, 0);
  ginsn_set_where (ginsn);

  return ginsn;
}

/* Generate ginsn for mov instructions with imm opnd.  */

static ginsnS *
aarch64_ginsn_mov_imm (const symbolS *insn_end_sym)
{
  ginsnS *ginsn = NULL;
  unsigned int src_reg = 0, dst_reg;
  aarch64_opnd_info *src, *dst;
  offsetT src_imm = 0;
  enum ginsn_src_type src_type;

  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  gas_assert (aarch64_num_of_operands (opcode) == 2);

  dst = &base->operands[0];
  src = &base->operands[1];

  /* For some mov ops, e.g., movn, movk, or movz, there may optionally be more
     work than just a simple mov.  Skip handling these mov altogether and let
     the aarch64_ginsn_unhandled () alert if these insns affect SCFI
     correctness.  TBD_GINSN_GEN_NOT_SCFI.  */
  if (src->type == AARCH64_OPND_HALF)
    return ginsn;

  dst_reg = ginsn_dw2_regnum (dst);
  /* FIXME Explicitly bar GINSN_TYPE_MOV which write to WZR / XZR at this time.
     This can be removed later when SCFI machinery is more robust to deal with
     GINSN_DW2_REGNUM_INVALID.  */
  if (ginsn_dw2_regnum_invalid_p (dst_reg))
    return ginsn;

  if (src->type == AARCH64_OPND_IMM_MOV
      && aarch64_gas_internal_fixup_p () && inst.reloc.exp.X_op == O_constant)
    {
      src_imm = inst.reloc.exp.X_add_number;
      src_type = GINSN_SRC_IMM;
    }
  else
    /* Skip now and handle via aarch64_ginsn_unhandled () code path.  */
    return ginsn;

  ginsn = ginsn_new_mov (insn_end_sym, false,
			 src_type, src_reg, src_imm,
			 GINSN_DST_REG, dst_reg, 0);
  ginsn_set_where (ginsn);

  return ginsn;
}

/* Check if an instruction is whitelisted.

   An instruction is a candidate for whitelisting if not generating ginsn for
   it, does not affect SCFI correctness.

   TBD_GINSN_GEN_NOT_SCFI.  This function assumes GINSN_GEN_SCFI is in effect.
   When other ginsn_gen_mode are added, this will need fixing.  */

static bool
aarch64_ginsn_safe_to_skip_p (void)
{
  bool skip_p = false;
  aarch64_opnd_info *opnd = NULL;
  unsigned int opnd_reg;
  int num_opnds = 0;
  bool dp_tag_only_p = false;

  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  /* ATM, whitelisting operations with no operands does not seem to be
     necessary.  In fact, whitelisting insns like ERET will be dangerous for
     SCFI.  So, return false now and bar any such insns from being whitelisted
     altogether.  */
  num_opnds = aarch64_num_of_operands (opcode);
  if (!num_opnds)
    return false;

  opnd = &base->operands[0];

  switch (opcode->iclass)
    {
    case ldst_regoff:
      /* It is not expected to have reg offset based ld/st ops to be used
	 for reg save and restore operations.  Warn the user though.  */
      opnd_reg = ginsn_dw2_regnum (opnd);
      if (aarch64_scfi_callee_saved_p (opnd_reg))
	{
	  skip_p = true;
	  as_warn ("SCFI: ignored probable save/restore op with reg offset");
	}
      break;

    case dp_2src:
      /* irg insn needs to be explicitly whitelisted.  This is because the
	 dest is Rd_SP, but irg insn affects the tag only.  To detect irg
	 insn, avoid an opcode-based check, however.  */
      dp_tag_only_p = aarch64_opcode_subclass_p (opcode, F_DP_TAG_ONLY);
      if (dp_tag_only_p)
	skip_p = true;
      break;

    default:
      break;
    }

  return skip_p;
}

enum aarch64_ginsn_unhandled_code
{
  AARCH64_GINSN_UNHANDLED_NONE,
  AARCH64_GINSN_UNHANDLED_DEST_REG,
  AARCH64_GINSN_UNHANDLED_CFG,
  AARCH64_GINSN_UNHANDLED_STACKOP,
  AARCH64_GINSN_UNHANDLED_UNEXPECTED,
};

/* Check the input insn for its impact on the correctness of the synthesized
   CFI.  Returns an error code to the caller.  */

static enum aarch64_ginsn_unhandled_code
aarch64_ginsn_unhandled (void)
{
  enum aarch64_ginsn_unhandled_code err = AARCH64_GINSN_UNHANDLED_NONE;
  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;
  aarch64_opnd_info *dest = &base->operands[0];
  int num_opnds = aarch64_num_of_operands (opcode);
  aarch64_opnd_info *addr;
  unsigned int dw2_regnum;
  unsigned int addr_reg;
  aarch64_opnd_info *opnd;
  unsigned int opnd_reg;

  /* All change of flow instructions (COFI) are important for SCFI.
     N.B. New iclasses for COFI when defined must be added here too.  */
  if (opcode->iclass == condbranch
      || opcode->iclass == compbranch
      || opcode->iclass == testbranch
      || opcode->iclass == branch_imm
      || opcode->iclass == branch_reg)
    err = AARCH64_GINSN_UNHANDLED_CFG;
  /* Also, any memory instructions that may involve an update to the stack
     pointer or save/restore of callee-saved registers must not be skipped.
     Note that, some iclasses cannot be used to push or pop stack because of
     disallowed writeback: ldst_unscaled, ldst_regoff, ldst_unpriv, ldstexcl,
     loadlit, ldstnapair_offs.  Except ldstnapair_offs from the afore-mentioned
     list, these iclasses do not seem to be amenable to being used for
     save/restore ops either.  */
  else if (opcode->iclass == ldstpair_off
	   || opcode->iclass == ldstnapair_offs
	   || opcode->iclass == ldstpair_indexed
	   || opcode->iclass == ldst_imm9
	   || opcode->iclass == ldst_imm10
	   || opcode->iclass == ldst_pos)
    {
      addr = &base->operands[num_opnds - 1];
      addr_reg = ginsn_dw2_regnum (addr);
      if (addr_reg == REG_SP || addr_reg == REG_FP)
	{
	  /* For all skipped memory operations, check if an update to REG_SP or
	     REG_FP is involved.  */
	  if ((addr->addr.postind || addr->addr.preind) && addr->addr.writeback)
	    err = AARCH64_GINSN_UNHANDLED_STACKOP;
	  /* Also check if a save / restore of a callee-saved register has been
	     missed.  */
	  else if (!aarch64_opcode_subclass_p (opcode, F_SUBCLASS_OTHER))
	    {
	      for (int i = 0; i < num_opnds - 1; i++)
		{
		  opnd = &base->operands[i];
		  opnd_reg = ginsn_dw2_regnum (opnd);
		  if (aarch64_scfi_callee_saved_p (opnd_reg)
		      && aarch64_get_qualifier_esize (opnd->qualifier) >= 8)
		    {
		      err = AARCH64_GINSN_UNHANDLED_STACKOP;
		      break;
		    }
		}
	    }
	}
    }
  /* STR Zn are especially complicated as they do not store in the same byte
     order for big-endian: STR Qn stores as a 128-bit integer (MSB first),
     whereas STR Zn stores as a stream of bytes (LSB first).  FIXME Simply punt
     on the big-endian and little-endian SVE PCS case for now.  */
  else if (opcode->iclass == sve_misc)
    {
      opnd = &base->operands[0];
      addr = &base->operands[num_opnds - 1];
      addr_reg = ginsn_dw2_regnum (addr);
      opnd_reg = ginsn_dw2_regnum (opnd);
      /* For all skipped memory operations, check if an update to REG_SP or
	 REG_FP is involved.  */
      if (aarch64_get_operand_class (addr->type) == AARCH64_OPND_CLASS_ADDRESS
	  && (addr_reg == REG_SP || addr_reg == REG_FP)
	  && (((addr->addr.postind || addr->addr.preind) && addr->addr.writeback)
	      || aarch64_scfi_callee_saved_p (opnd_reg)))
	err = AARCH64_GINSN_UNHANDLED_STACKOP;
    }

  /* Finally, irrespective of the iclass, check if the missed instructions are
     affecting REG_SP or REG_FP.  */
  else if (dest && (dest->type == AARCH64_OPND_Rd
		    || dest->type == AARCH64_OPND_Rd_SP))
    {
      dw2_regnum = ginsn_dw2_regnum (dest);

      if (dw2_regnum == REG_SP || dw2_regnum == REG_FP)
	err = AARCH64_GINSN_UNHANDLED_DEST_REG;
    }

  return err;
}

/* Generate one or more generic GAS instructions, a.k.a, ginsns for the
   current machine instruction.

   Returns the head of linked list of ginsn(s) added, if success; Returns NULL
   if failure.

   The input ginsn_gen_mode GMODE determines the set of minimal necessary
   ginsns necessary for correctness of any passes applicable for that mode.
   For supporting the GINSN_GEN_SCFI generation mode, following is the list of
   machine instructions that must be translated into the corresponding ginsns
   to ensure correctness of SCFI:
     - All instructions affecting the two registers that could potentially
       be used as the base register for CFA tracking.  For SCFI, the base
       register for CFA tracking is limited to REG_SP and REG_FP only.
     - All change of flow instructions: conditional and unconditional
       branches, call and return from functions.
     - All instructions that can potentially be a register save / restore
       operations.
     - All instructions that may update the stack pointer: pre-indexed and
       post-indexed stack operations with writeback.

   The function currently supports GINSN_GEN_SCFI ginsn generation mode only.
   To support other generation modes will require work on this target-specific
   process of creation of ginsns:
     - Some of such places are tagged with TBD_GINSN_GEN_NOT_SCFI to serve as
       possible starting points.
     - Also note that ginsn representation may need enhancements.  Specifically,
       note some TBD_GINSN_INFO_LOSS and TBD_GINSN_REPRESENTATION_LIMIT markers.
   */

static ginsnS *
aarch64_ginsn_new (const symbolS *insn_end_sym, enum ginsn_gen_mode gmode)
{
  enum aarch64_ginsn_unhandled_code err = 0;
  ginsnS *ginsn = NULL;
  unsigned int dw2_regnum;
  aarch64_opnd_info *dest = NULL;
  aarch64_inst *base = &inst.base;
  const aarch64_opcode *opcode = base->opcode;

  /* Currently supports generation of selected ginsns, sufficient for
     the use-case of SCFI only.  To remove this condition will require
     work on this target-specific process of creation of ginsns.  Some
     of such places are tagged with TBD_GINSN_GEN_NOT_SCFI to serve as
     examples.  */
  if (gmode != GINSN_GEN_SCFI)
    return ginsn;

  switch (opcode->iclass)
    {
    case addsub_ext:
      /* TBD_GINSN_GEN_NOT_SCFI: other insns are not of interest for SCFI.  */
      if (aarch64_opcode_subclass_p (opcode, F_ARITH_ADD)
	   || aarch64_opcode_subclass_p (opcode, F_ARITH_SUB))
	ginsn = aarch64_ginsn_addsub_reg (insn_end_sym);
      break;

    case addsub_imm:
      if (aarch64_opcode_subclass_p (opcode, F_ARITH_MOV))
	ginsn = aarch64_ginsn_mov_reg (insn_end_sym);
      else if (aarch64_opcode_subclass_p (opcode, F_ARITH_ADD)
	       || aarch64_opcode_subclass_p (opcode, F_ARITH_SUB))
	ginsn = aarch64_ginsn_addsub_imm (insn_end_sym);
      /* Note how addg, subg involving tags have F_SUBCLASS_OTHER flag.  These
	 insns will see a GINSN_TYPE_OTHER created for them if the destination
	 register is of interest via the aarch64_ginsn_unhandled ()
	 codepath.  */
      break;

    case movewide:
      ginsn = aarch64_ginsn_mov_imm (insn_end_sym);
      break;

    case ldst_imm9:
    case ldst_pos:
      ginsn = aarch64_ginsn_ldstr (insn_end_sym);
      break;

    case ldstpair_indexed:
    case ldstpair_off:
    case ldstnapair_offs:
      ginsn = aarch64_ginsn_ldstp (insn_end_sym);
      break;

    case branch_imm:
    case branch_reg:
      ginsn = aarch64_ginsn_branch_uncond (insn_end_sym);
      break;

    case compbranch:
      /* Although cbz/cbnz has an additional operand and are functionally
	 distinct from conditional branches, it is fine to use the same ginsn
	 type for both from the perspective of SCFI.  */
    case testbranch:
    case condbranch:
      ginsn = aarch64_ginsn_branch_cond (insn_end_sym);
      break;

    default:
      /* TBD_GINSN_GEN_NOT_SCFI: Skip all other opcodes uninteresting for
	 GINSN_GEN_SCFI mode.  */
      break;
    }

  if (!ginsn && !aarch64_ginsn_safe_to_skip_p ())
    {
      /* For all unhandled insns, check that they no not impact SCFI
	 correctness.  */
      err = aarch64_ginsn_unhandled ();
      switch (err)
	{
	case AARCH64_GINSN_UNHANDLED_NONE:
	  break;
	case AARCH64_GINSN_UNHANDLED_DEST_REG:
	  /* Not all writes to REG_FP are harmful in context of SCFI.  Simply
	     generate a GINSN_TYPE_OTHER with destination set to the
	     appropriate register.  The SCFI machinery will bail out if this
	     ginsn affects SCFI correctness.  */
	  dest = &base->operands[0];
	  dw2_regnum = ginsn_dw2_regnum (dest);
	  /* Sanity check.  */
	  gas_assert (!ginsn_dw2_regnum_invalid_p (dw2_regnum));
	  ginsn = ginsn_new_other (insn_end_sym, true,
				   GINSN_SRC_IMM, 0,
				   GINSN_SRC_IMM, 0,
				   GINSN_DST_REG, dw2_regnum);
	  ginsn_set_where (ginsn);
	  break;
	case AARCH64_GINSN_UNHANDLED_CFG:
	case AARCH64_GINSN_UNHANDLED_STACKOP:
	  as_bad (_("SCFI: unhandled op %#x may cause incorrect CFI"),
		  opcode->opcode);
	  break;
	case AARCH64_GINSN_UNHANDLED_UNEXPECTED:
	  as_bad (_("SCFI: unexpected op %#x may cause incorrect CFI"),
		  opcode->opcode);
	  break;
	default:
	  abort ();
	  break;
	}
    }

  return ginsn;
}

#endif /* OBJ_ELF.  */

