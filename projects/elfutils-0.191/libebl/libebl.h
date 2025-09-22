/* Interface for libebl.
   Copyright (C) 2000-2010, 2013, 2014, 2015, 2016, 2017 Red Hat, Inc.
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


/* This is the interface for the Elfutils Backend Library.
   It is a completely UNSUPPORTED interface.  Don't use any libebl
   function directly.  These are only for internal elfutils backends
   and tools.  There is NO source or binary compatible guarantee.  */


#ifndef _LIBEBL_H
#define _LIBEBL_H 1

#include <gelf.h>
#include "libdw.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "elf-knowledge.h"


/* Opaque type for the handle.  libasm.h defined the same thing.  */
#ifndef _LIBASM_H
typedef struct ebl Ebl;
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Get backend handle for object associated with ELF handle.  */
extern Ebl *ebl_openbackend (Elf *elf);
/* Similar but without underlying ELF file.  */
extern Ebl *ebl_openbackend_machine (GElf_Half machine);
/* Similar but with emulation name given.  */
extern Ebl *ebl_openbackend_emulation (const char *emulation);

/* Free resources allocated for backend handle.  */
extern void ebl_closebackend (Ebl *bh);


/* Information about the descriptor.  */

/* Get ELF machine.  */
extern int ebl_get_elfmachine (Ebl *ebl) __pure_attribute__;

/* Get ELF class.  */
extern int ebl_get_elfclass (Ebl *ebl) __pure_attribute__;

/* Get ELF data encoding.  */
extern int ebl_get_elfdata (Ebl *ebl) __pure_attribute__;


/* Function to call the callback functions including default ELF
   handling.  */

/* Return backend name.  */
extern const char *ebl_backend_name (Ebl *ebl);

/* Return relocation type name.  */
extern const char *ebl_reloc_type_name (Ebl *ebl, int reloc,
					char *buf, size_t len);

/* Check relocation type.  */
extern bool ebl_reloc_type_check (Ebl *ebl, int reloc);

/* Check relocation type use.  */
extern bool ebl_reloc_valid_use (Ebl *ebl, int reloc);

/* Check if relocation type is for simple absolute relocations.
   Return ELF_T_{BYTE,HALF,SWORD,SXWORD} for a simple type, else ELF_T_NUM.
   If the relocation type is an ADD or SUB relocation, set *ADDSUB to 1 or -1,
   resp.  */
extern Elf_Type ebl_reloc_simple_type (Ebl *ebl, int reloc, int *addsub);

/* Return true if the symbol type is that referencing the GOT.  E.g.,
   R_386_GOTPC.  */
extern bool ebl_gotpc_reloc_check (Ebl *ebl, int reloc);

/* Return segment type name.  */
extern const char *ebl_segment_type_name (Ebl *ebl, int segment,
					  char *buf, size_t len);

/* Return section type name.  */
extern const char *ebl_section_type_name (Ebl *ebl, int section,
					  char *buf, size_t len);

/* Return section name.  */
extern const char *ebl_section_name (Ebl *ebl, int section, int xsection,
				     char *buf, size_t len,
				     const char *scnnames[], size_t shnum);

/* Return machine flag names.  */
extern const char *ebl_machine_flag_name (Ebl *ebl, GElf_Word flags,
					  char *buf, size_t len);

/* Check whether machine flag is valid.  */
extern bool ebl_machine_flag_check (Ebl *ebl, GElf_Word flags);

/* Check whether SHF_MASKPROC flags are valid.  */
extern bool ebl_machine_section_flag_check (Ebl *ebl, GElf_Xword flags);

/* Check whether the section with the given index, header, and name
   is a special machine section that is valid despite a combination
   of flags or other details that are not generically valid.  */
extern bool ebl_check_special_section (Ebl *ebl, int ndx,
				       const GElf_Shdr *shdr, const char *name);

/* Return symbol type name.  */
extern const char *ebl_symbol_type_name (Ebl *ebl, int symbol,
					 char *buf, size_t len);

/* Return symbol binding name.  */
extern const char *ebl_symbol_binding_name (Ebl *ebl, int binding,
					    char *buf, size_t len);

/* Return dynamic tag name.  */
extern const char *ebl_dynamic_tag_name (Ebl *ebl, int64_t tag,
					 char *buf, size_t len);

/* Check dynamic tag.  */
extern bool ebl_dynamic_tag_check (Ebl *ebl, int64_t tag);

/* Check whether given symbol's st_value and st_size are OK despite failing
   normal checks.  */
extern bool ebl_check_special_symbol (Ebl *ebl,
				      const GElf_Sym *sym, const char *name,
				      const GElf_Shdr *destshdr);

/* Check if this is a data marker symbol.  e.g. '$d' symbols for ARM.  */
extern bool ebl_data_marker_symbol (Ebl *ebl, const GElf_Sym *sym,
				    const char *sname);

/* Check whether only valid bits are set on the st_other symbol flag.  */
extern bool ebl_check_st_other_bits (Ebl *ebl, unsigned char st_other);

/* Return symbolic representation of OS ABI.  */
extern const char *ebl_osabi_name (Ebl *ebl, int osabi, char *buf, size_t len);


/* Return name of the note section type for a core file.  */
extern const char *ebl_core_note_type_name (Ebl *ebl, uint32_t type, char *buf,
					    size_t len);

/* Return name of the note section type for an object file.  */
extern const char *ebl_object_note_type_name (Ebl *ebl, const char *name,
					      uint32_t type, GElf_Word descsz,
					      char *buf, size_t len);

/* Print information about object note if available.  */
extern void ebl_object_note (Ebl *ebl, uint32_t namesz, const char *name,
			     uint32_t type, uint32_t descsz, const char *desc);

/* Check whether an attribute in a .gnu_attributes section is recognized.
   Fills in *TAG_NAME with the name for this tag.
   If VALUE is a known value for that tag, also fills in *VALUE_NAME.  */
extern bool ebl_check_object_attribute (Ebl *ebl, const char *vendor,
					int tag, uint64_t value,
					const char **tag_name,
					const char **value_name);

/* Check whether a section type is a valid reloc target.  */
extern bool ebl_check_reloc_target_type (Ebl *ebl, Elf64_Word sh_type);


/* Check section name for being that of a debug informatino section.  */
extern bool ebl_debugscn_p (Ebl *ebl, const char *name);

/* Check whether given relocation is a copy relocation.  */
extern bool ebl_copy_reloc_p (Ebl *ebl, int reloc);

/* Check whether given relocation is a no-op relocation.  */
extern bool ebl_none_reloc_p (Ebl *ebl, int reloc);

/* Check whether given relocation is a relative relocation.  */
extern bool ebl_relative_reloc_p (Ebl *ebl, int reloc);

/* Check whether section should be stripped.  */
extern bool ebl_section_strip_p (Ebl *ebl,
				 const GElf_Shdr *shdr, const char *name,
				 bool remove_comment, bool only_remove_debug);

/* Check if backend uses a bss PLT in this file.  */
extern bool ebl_bss_plt_p (Ebl *ebl);

/* Return size of entry in SysV-style hash table.  */
extern int ebl_sysvhash_entrysize (Ebl *ebl);

/* Return location expression to find return value given a
   DW_TAG_subprogram, DW_TAG_subroutine_type, or similar DIE describing
   function itself (whose DW_AT_type attribute describes its return type).
   Returns -1 for a libdw error (see dwarf_errno).
   Returns -2 for an unrecognized type formation.
   Returns zero if the function has no return value (e.g. "void" in C).
   Otherwise, *LOCOPS gets a location expression to find the return value,
   and returns the number of operations in the expression.  The pointer is
   permanently allocated at least as long as the Ebl handle is open.  */
extern int ebl_return_value_location (Ebl *ebl,
				      Dwarf_Die *functypedie,
				      const Dwarf_Op **locops);

/* Fill in register information given DWARF register numbers.
   If NAME is null, return the maximum REGNO + 1 that has a name.
   Otherwise, store in NAME the name for DWARF register number REGNO
   and return the number of bytes written (including '\0' terminator).
   Return -1 if NAMELEN is too short or REGNO is negative or too large.
   Return 0 if REGNO is unused (a gap in the DWARF number assignment).
   On success, set *SETNAME to a description like "integer" or "FPU"
   fit for "%s registers" title display, and *PREFIX to the string
   that precedes NAME in canonical assembler syntax (e.g. "%" or "$").
   The NAME string contains identifier characters only (maybe just digits).  */
extern ssize_t ebl_register_info (Ebl *ebl,
				  int regno, char *name, size_t namelen,
				  const char **prefix, const char **setname,
				  int *bits, int *type);

/* Supply the ABI-specified state of DWARF CFI before CIE initial programs.

   The DWARF 3.0 spec says that the default initial states of all registers
   are "undefined", unless otherwise specified by the machine/compiler ABI.

   This default is wrong for every machine with the CFI generated by GCC.
   The EH unwinder does not really distinguish "same_value" and "undefined",
   since it doesn't matter for unwinding (in either case there is no change
   to make for that register).  GCC generates CFI that says nothing at all
   about registers it hasn't spilled somewhere.  For our unwinder to give
   the true story, the backend must supply an initial state that uses
   "same_value" rules for all the callee-saves registers.

   This can fill in the initial_instructions, initial_instructions_end
   members of *ABI_INFO to point at a CFI instruction stream to process
   before each CIE's initial instructions.  It should set the
   data_alignment_factor member if it affects the initial instructions.

   The callback should not use the register rules DW_CFA_expression or
   DW_CFA_val_expression.  Defining the CFA using DW_CFA_def_cfa_expression
   is allowed.  This is an implementation detail since register rules
   store expressions as offsets from the .eh_frame or .debug_frame data.

   As a shorthand for some common cases, for this instruction stream
   we overload some CFI instructions that cannot be used in a CIE:

	DW_CFA_restore		-- Change default rule for all unmentioned
				   registers from undefined to same_value.

   This function can also fill in ABI_INFO->return_address_register with the
   DWARF register number that identifies the actual PC in machine state.
   If there is no canonical DWARF register number with that meaning, it's
   left unchanged (callers usually initialize with (Dwarf_Word) -1).
   This value is not used by CFI per se.

   Function returns 0 on success and -1 for error or unsupported by the
   backend.  */
extern int ebl_abi_cfi (Ebl *ebl, Dwarf_CIE *abi_info)
  __nonnull_attribute__ (2);

/* Register map info. */
typedef struct
{
  Dwarf_Half offset;		/* Byte offset in register data block.  */
  Dwarf_Half regno;		/* DWARF register number.  */
  uint8_t bits;			/* Bits of data for one register.  */
  uint8_t pad;			/* Bytes of padding after register's data.  */
  Dwarf_Half count;		/* Consecutive register numbers here.  */
  bool pc_register;
} Ebl_Register_Location;

/* Non-register data items in core notes.  */
typedef struct
{
  const char *name;		/* Printable identifier.  */
  const char *group;		/* Identifier for category of related items.  */
  Dwarf_Half offset;		/* Byte offset in note data.  */
  Dwarf_Half count;
  Elf_Type type;
  char format;
  bool thread_identifier;
  bool pc_register;
} Ebl_Core_Item;

/* Describe the format of a core file note with the given header and NAME.
   NAME is not guaranteed terminated, it's NHDR->n_namesz raw bytes.  */
extern int ebl_core_note (Ebl *ebl, const GElf_Nhdr *nhdr,
			  const char *name, const char *desc,
			  GElf_Word *regs_offset, size_t *nregloc,
			  const Ebl_Register_Location **reglocs,
			  size_t *nitems, const Ebl_Core_Item **items)
  __nonnull_attribute__ (1, 2, 3, 4, 5, 6, 7, 8);

/* Describe the auxv type number.  */
extern int ebl_auxv_info (Ebl *ebl, GElf_Xword a_type,
			  const char **name, const char **format)
  __nonnull_attribute__ (1, 3, 4);

/* Callback type for ebl_set_initial_registers_tid.
   Register -1 is mapped to PC (if arch PC has no DWARF number).
   If FIRSTREG is -1 then NREGS has to be 1.  */
typedef bool (ebl_tid_registers_t) (int firstreg, unsigned nregs,
				    const Dwarf_Word *regs, void *arg)
  __nonnull_attribute__ (3);

/* Callback to fetch process data from live TID.
   EBL architecture has to have EBL_FRAME_NREGS > 0, otherwise the
   backend doesn't support unwinding and this function call may crash.  */
extern bool ebl_set_initial_registers_tid (Ebl *ebl,
					   pid_t tid,
					   ebl_tid_registers_t *setfunc,
					   void *arg)
  __nonnull_attribute__ (1, 3);

/* Number of registers to allocate for ebl_set_initial_registers_tid.
   EBL architecture can unwind iff EBL_FRAME_NREGS > 0.  */
extern size_t ebl_frame_nregs (Ebl *ebl)
  __nonnull_attribute__ (1);

/* Offset to apply to the value of the return_address_register, as
   fetched from a Dwarf CFI.  This is used by some backends, where the
   return_address_register actually contains the call address.  */
extern int ebl_ra_offset (Ebl *ebl)
  __nonnull_attribute__ (1);

/* Mask to use for function symbol or unwind return addresses in case
   the architecture adds some extra non-address bits to it.  This is
   different from ebl_resolve_sym_value which only works for actual
   symbol addresses (in non-ET_REL files) that might resolve to an
   address in a different section.  ebl_func_addr_mask is called to
   turn a given function value into the a real address or offset (the
   original value might not be a real address).  This works for all
   cases where an actual function address (or offset in ET_REL symbol
   tables) is needed.  */
extern GElf_Addr ebl_func_addr_mask (Ebl *ebl);

/* Convert *REGNO as is in DWARF to a lower range suitable for
   Dwarf_Frame->REGS indexing.  */
extern bool ebl_dwarf_to_regno (Ebl *ebl, unsigned *regno)
  __nonnull_attribute__ (1, 2);

/* Modify PC as fetched from inferior data into valid PC.  */
extern void ebl_normalize_pc (Ebl *ebl, Dwarf_Addr *pc)
  __nonnull_attribute__ (1, 2);

/* Callback type for ebl_unwind's parameter getfunc.  */
typedef bool (ebl_tid_registers_get_t) (int firstreg, unsigned nregs,
					Dwarf_Word *regs, void *arg)
  __nonnull_attribute__ (3);

/* Callback type for ebl_unwind's parameter readfunc.  */
typedef bool (ebl_pid_memory_read_t) (Dwarf_Addr addr, Dwarf_Word *data,
				      void *arg)
  __nonnull_attribute__ (3);

/* Get previous frame state for an existing frame state.  Method is called only
   if unwinder could not find CFI for current PC.  PC is for the
   existing frame.  SETFUNC sets register in the previous frame.  GETFUNC gets
   register from the existing frame.  Note that GETFUNC vs. SETFUNC act on
   a disjunct set of registers.  READFUNC reads memory.  ARG has to be passed
   for SETFUNC, GETFUNC and READFUNC.  *SIGNAL_FRAMEP is initialized to false,
   it can be set to true if existing frame is a signal frame.  SIGNAL_FRAMEP is
   never NULL.  */
extern bool ebl_unwind (Ebl *ebl, Dwarf_Addr pc, ebl_tid_registers_t *setfunc,
			ebl_tid_registers_get_t *getfunc,
			ebl_pid_memory_read_t *readfunc, void *arg,
			bool *signal_framep)
  __nonnull_attribute__ (1, 3, 4, 5, 7);

/* Returns true if the value can be resolved to an address in an
   allocated section, which will be returned in *ADDR
   (e.g. function descriptor resolving)  */
extern bool ebl_resolve_sym_value (Ebl *ebl, GElf_Addr *addr)
   __nonnull_attribute__ (2);

#ifdef __cplusplus
}
#endif

#endif	/* libebl.h */
