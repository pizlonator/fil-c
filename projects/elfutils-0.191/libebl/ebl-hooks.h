/* Backend hook signatures internal interface for libebl.
   Copyright (C) 2000-2011, 2013, 2014, 2016, 2017 Red Hat, Inc.
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

/* Return symbolic representation of relocation type.  */
const char *EBLHOOK(reloc_type_name) (int, char *, size_t);

/* Check relocation type.  */
bool EBLHOOK(reloc_type_check) (int);

/* Check if relocation type is for simple absolute relocations.  */
Elf_Type EBLHOOK(reloc_simple_type) (Ebl *, int, int *);

/* Check relocation type use.  */
bool EBLHOOK(reloc_valid_use) (Elf *, int);

/* Return true if the symbol type is that referencing the GOT.  */
bool EBLHOOK(gotpc_reloc_check) (Elf *, int);

/* Return symbolic representation of segment type.  */
const char *EBLHOOK(segment_type_name) (int, char *, size_t);

/* Return symbolic representation of section type.  */
const char *EBLHOOK(section_type_name) (int, char *, size_t);

/* Return section name.  */
const char *EBLHOOK(section_name) (int, int, char *, size_t);

/* Return next machine flag name.  */
const char *EBLHOOK(machine_flag_name) (GElf_Word, GElf_Word *);

/* Check whether machine flags are valid.  */
bool EBLHOOK(machine_flag_check) (GElf_Word);

/* Check whether SHF_MASKPROC flag bits are valid.  */
bool EBLHOOK(machine_section_flag_check) (GElf_Xword);

/* Check whether the section with the given index, header, and name
   is a special machine section that is valid despite a combination
   of flags or other details that are not generically valid.  */
bool EBLHOOK(check_special_section) (Ebl *, int,
				     const GElf_Shdr *, const char *);

/* Return symbolic representation of symbol type.  */
const char *EBLHOOK(symbol_type_name) (int, char *, size_t);

/* Return symbolic representation of symbol binding.  */
const char *EBLHOOK(symbol_binding_name) (int, char *, size_t);

/* Return symbolic representation of dynamic tag.  */
const char *EBLHOOK(dynamic_tag_name) (int64_t, char *, size_t);

/* Check dynamic tag.  */
bool EBLHOOK(dynamic_tag_check) (int64_t);

/* Return symbolic representation of OS ABI.  */
const char *EBLHOOK(osabi_name) (int, char *, size_t);

/* Name of a note entry type for core files.  */
const char *EBLHOOK(core_note_type_name) (uint32_t, char *, size_t);

/* Name of a note entry type for object files.  */
const char *EBLHOOK(object_note_type_name) (const char *, uint32_t,
					    char *, size_t);

/* Describe core note format.  */
int EBLHOOK(core_note) (const GElf_Nhdr *, const char *,
			GElf_Word *, size_t *, const Ebl_Register_Location **,
			size_t *, const Ebl_Core_Item **);

/* Handle object file note.  */
bool EBLHOOK(object_note) (const char *, uint32_t, uint32_t, const char *);

/* Check object attribute.  */
bool EBLHOOK(check_object_attribute) (Ebl *, const char *, int, uint64_t,
				      const char **, const char **);

/* Check reloc target section type.  */
bool EBLHOOK(check_reloc_target_type) (Ebl *, Elf64_Word);

/* Describe auxv element type.  */
int EBLHOOK(auxv_info) (GElf_Xword, const char **, const char **);

/* Check section name for being that of a debug informatino section.  */
bool EBLHOOK(debugscn_p) (const char *);

/* Check whether given relocation is a copy relocation.  */
bool EBLHOOK(copy_reloc_p) (int);

/* Check whether given relocation is a no-op relocation.  */
bool EBLHOOK(none_reloc_p) (int);

/* Check whether given relocation is a relative relocation.  */
bool EBLHOOK(relative_reloc_p) (int);

/* Check whether given symbol's value is ok despite normal checks.  */
bool EBLHOOK(check_special_symbol) (Elf *, const GElf_Sym *,
			      const char *, const GElf_Shdr *);

/* Check if this is a data marker symbol.  e.g. '$d' symbols for ARM.  */
bool EBLHOOK(data_marker_symbol) (const GElf_Sym *sym, const char *sname);

/* Check whether only valid bits are set on the st_other symbol flag.
   Standard ST_VISIBILITY have already been masked off.  */
bool EBLHOOK(check_st_other_bits) (unsigned char st_other);

/* Check if backend uses a bss PLT in this file.  */
bool EBLHOOK(bss_plt_p) (Elf *);

/* Return location expression to find return value given the
   DW_AT_type DIE of a DW_TAG_subprogram DIE.  */
int EBLHOOK(return_value_location) (Dwarf_Die *functypedie,
				    const Dwarf_Op **locp);

/* Return register name information.  */
ssize_t EBLHOOK(register_info) (Ebl *ebl,
				int regno, char *name, size_t namelen,
				const char **prefix, const char **setname,
				int *bits, int *type);

/* Disassembler function.  */
int EBLHOOK(disasm) (Ebl *ebl, const uint8_t **startp, const uint8_t *end,
		     GElf_Addr addr, const char *fmt, DisasmOutputCB_t outcb,
		     DisasmGetSymCB_t symcb, void *outcbarg, void *symcbarg);

/* Supply the machine-specific state of CFI before CIE initial programs.
   Function returns 0 on success and -1 on error.  */
int EBLHOOK(abi_cfi) (Ebl *ebl, Dwarf_CIE *abi_info);

/* Fetch process data from live TID and call SETFUNC one or more times.
   Method should be present only when EBL_FRAME_NREGS > 0, otherwise the
   backend doesn't support unwinding.  */
bool EBLHOOK(set_initial_registers_tid) (pid_t tid,
					 ebl_tid_registers_t *setfunc,
					 void *arg);

/* Convert *REGNO as is in DWARF to a lower range suitable for
   Dwarf_Frame->REGS indexing.  */
bool EBLHOOK(dwarf_to_regno) (Ebl *ebl, unsigned *regno);

/* Optionally modify *PC as fetched from inferior data into valid PC
   instruction pointer.  */
void EBLHOOK(normalize_pc) (Ebl *ebl, Dwarf_Addr *pc);

/* Get previous frame state for an existing frame state.  Method is called only
   if unwinder could not find CFI for current PC.  PC is for the
   existing frame.  SETFUNC sets register in the previous frame.  GETFUNC gets
   register from the existing frame.  Note that GETFUNC vs. SETFUNC act on
   a disjunct set of registers.  READFUNC reads memory.  ARG has to be passed
   for SETFUNC, GETFUNC and READFUNC.  *SIGNAL_FRAMEP is initialized to false,
   it can be set to true if existing frame is a signal frame.  SIGNAL_FRAMEP is
   never NULL.  */
bool EBLHOOK(unwind) (Ebl *ebl, Dwarf_Addr pc, ebl_tid_registers_t *setfunc,
		      ebl_tid_registers_get_t *getfunc,
		      ebl_pid_memory_read_t *readfunc, void *arg,
		      bool *signal_framep);

/* Returns true if the value can be resolved to an address in an
   allocated section, which will be returned in *ADDR.
   (e.g. function descriptor resolving)  */
bool EBLHOOK(resolve_sym_value) (Ebl *ebl, GElf_Addr *addr);

/* Destructor for ELF backend handle.  */
void EBLHOOK(destr) (struct ebl *);
