/* Internal definitions for libdw CFI interpreter.
   Copyright (C) 2009-2010, 2013, 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef _UNWINDP_H
#define _UNWINDP_H 1

#include "libdwP.h"
#include "libelfP.h"
struct ebl;

/* Cached CIE representation.  */
struct dwarf_cie
{
  Dwarf_Off offset;	 /* Our position, as seen in FDEs' CIE_pointer.  */

  Dwarf_Word code_alignment_factor;
  Dwarf_Sword data_alignment_factor;
  Dwarf_Word return_address_register;

  size_t fde_augmentation_data_size;

  // play out to initial state
  const uint8_t *initial_instructions;
  const uint8_t *initial_instructions_end;

  const Dwarf_Frame *initial_state;

  uint8_t fde_encoding;		/* DW_EH_PE_* for addresses in FDEs.  */
  uint8_t lsda_encoding;    /* DW_EH_PE_* for LSDA in FDE augmentation.  */

  bool sized_augmentation_data;	/* Saw 'z': FDEs have self-sized data.  */
  bool signal_frame;		/* Saw 'S': FDE is for a signal frame.  */
};

/* Cached FDE representation.  */
struct dwarf_fde
{
  struct dwarf_cie *cie;

  /* This FDE describes PC values in [start, end).  */
  Dwarf_Addr start;
  Dwarf_Addr end;

  const uint8_t *instructions;
  const uint8_t *instructions_end;
};

/* This holds everything we cache about the CFI from each ELF file's
   .debug_frame or .eh_frame section.  */
struct Dwarf_CFI_s
{
  /* Dwarf handle we came from.  If null, this is .eh_frame data.  */
  Dwarf *dbg;
#define CFI_IS_EH(cfi)	((cfi)->dbg == NULL)

  /* Data of the .debug_frame or .eh_frame section.  */
  Elf_Data_Scn *data;
  const unsigned char *e_ident;	/* For EI_DATA and EI_CLASS.  */

  Dwarf_Addr frame_vaddr;  /* DW_EH_PE_pcrel, address of frame section.  */
  Dwarf_Addr textrel;		/* DW_EH_PE_textrel base address.  */
  Dwarf_Addr datarel;		/* DW_EH_PE_datarel base address.  */

  /* Location of next unread entry in the section.  */
  Dwarf_Off next_offset;

  /* Search tree for the CIEs, indexed by CIE_pointer (section offset).  */
  void *cie_tree;

  /* Search tree for the FDEs, indexed by PC address.  */
  void *fde_tree;

  /* Search tree for parsed DWARF expressions, indexed by raw pointer.  */
  void *expr_tree;

  /* Backend hook.  */
  struct ebl *ebl;

  /* Binary search table in .eh_frame_hdr section.  */
  const uint8_t *search_table;
  size_t search_table_len;
  Dwarf_Addr search_table_vaddr;
  size_t search_table_entries;
  uint8_t search_table_encoding;

  uint16_t e_machine;

  /* True if the file has a byte order different from the host.  */
  bool other_byte_order;

  /* Default rule for registers not previously mentioned
     is same_value, not undefined.  */
  bool default_same_value;
};


enum dwarf_frame_rule
  {
    reg_unspecified,		/* Uninitialized state.  */
    reg_undefined,		/* DW_CFA_undefined */
    reg_same_value,		/* DW_CFA_same_value */
    reg_offset,			/* DW_CFA_offset_extended et al */
    reg_val_offset,		/* DW_CFA_val_offset et al */
    reg_register,		/* DW_CFA_register */
    reg_expression,		/* DW_CFA_expression */
    reg_val_expression,		/* DW_CFA_val_expression */
  };

/* This describes what we know about an individual register.  */
struct dwarf_frame_register
{
  enum dwarf_frame_rule rule:3;

  /* The meaning of the value bits depends on the rule:

	Rule			Value
	----			-----
	undefined		unused
	same_value		unused
	offset(N)		N	(register saved at CFA + value)
	val_offset(N)		N	(register = CFA + value)
	register(R)		R	(register = register #value)
	expression(E)		section offset of DW_FORM_block containing E
					(register saved at address E computes)
	val_expression(E)	section offset of DW_FORM_block containing E
					(register = value E computes)
  */
  Dwarf_Sword value:(sizeof (Dwarf_Sword) * 8 - 3);
};

/* This holds instructions for unwinding frame at a particular PC location
   described by an FDE.  */
struct Dwarf_Frame_s
{
  /* This frame description covers PC values in [start, end).  */
  Dwarf_Addr start;
  Dwarf_Addr end;

  Dwarf_CFI *cache;

  /* Previous state saved by DW_CFA_remember_state, or .cie->initial_state,
     or NULL in an initial_state pseudo-frame.  */
  Dwarf_Frame *prev;

  /* The FDE that generated this frame state.  This points to its CIE,
     which has the return_address_register and signal_frame flag.  */
  struct dwarf_fde *fde;

  /* The CFA is unknown, is R+N, or is computed by a DWARF expression.
     A bogon in the CFI can indicate an invalid/incalculable rule.
     We store that as cfa_invalid rather than barfing when processing it,
     so callers can ignore the bogon unless they really need that CFA.  */
  enum { cfa_undefined, cfa_offset, cfa_expr, cfa_invalid } cfa_rule;
  union
  {
    Dwarf_Op offset;
    Dwarf_Block expr;
  } cfa_data;
  /* We store an offset rule as a DW_OP_bregx operation.  */
#define cfa_val_reg	cfa_data.offset.number
#define cfa_val_offset	cfa_data.offset.number2

  size_t nregs;
  struct dwarf_frame_register regs[];
};


/* Clean up the data structure and all it points to.  */
extern void __libdw_destroy_frame_cache (Dwarf_CFI *cache)
  __nonnull_attribute__ (1) internal_function;

/* Enter a CIE encountered while reading through for FDEs.  */
extern void __libdw_intern_cie (Dwarf_CFI *cache, Dwarf_Off offset,
				const Dwarf_CIE *info)
  __nonnull_attribute__ (1, 3) internal_function;

/* Look up a CIE_pointer for random access.  */
extern struct dwarf_cie *__libdw_find_cie (Dwarf_CFI *cache, Dwarf_Off offset)
  __nonnull_attribute__ (1) internal_function;


/* Look for an FDE covering the given PC address.  */
extern struct dwarf_fde *__libdw_find_fde (Dwarf_CFI *cache,
					   Dwarf_Addr address)
  __nonnull_attribute__ (1) internal_function;

/* Look for an FDE by its offset in the section.  */
extern struct dwarf_fde *__libdw_fde_by_offset (Dwarf_CFI *cache,
						Dwarf_Off offset)
  __nonnull_attribute__ (1) internal_function;

/* Process the FDE that contains the given PC address,
   to yield the frame state when stopped there.
   The return value is a DWARF_E_* error code.  */
extern int __libdw_frame_at_address (Dwarf_CFI *cache, struct dwarf_fde *fde,
				     Dwarf_Addr address, Dwarf_Frame **frame)
  __nonnull_attribute__ (1, 2, 4) internal_function;


/* Dummy struct for memory-access.h macros.  */
#define BYTE_ORDER_DUMMY(var, e_ident)					      \
  const struct { bool other_byte_order; } var =				      \
    { ((BYTE_ORDER == LITTLE_ENDIAN && e_ident[EI_DATA] == ELFDATA2MSB)       \
       || (BYTE_ORDER == BIG_ENDIAN && e_ident[EI_DATA] == ELFDATA2LSB)) }


INTDECL (dwarf_next_cfi)
INTDECL (dwarf_getcfi)
INTDECL (dwarf_getcfi_elf)
INTDECL (dwarf_cfi_end)
INTDECL (dwarf_cfi_addrframe)

#endif	/* unwindP.h */
