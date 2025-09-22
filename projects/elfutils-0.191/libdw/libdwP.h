/* Internal definitions for libdw.
   Copyright (C) 2002-2011, 2013-2018 Red Hat, Inc.
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

#ifndef _LIBDWP_H
#define _LIBDWP_H 1

#include <stdbool.h>
#include <pthread.h>

#include <libdw.h>
#include <dwarf.h>


/* Known location expressions already decoded.  */
struct loc_s
{
  void *addr;
  Dwarf_Op *loc;
  size_t nloc;
};

/* Known DW_OP_implicit_value blocks already decoded.
   This overlaps struct loc_s exactly, but only the
   first member really has to match.  */
struct loc_block_s
{
  void *addr;
  unsigned char *data;
  size_t length;
};

/* Already decoded .debug_line units.  */
struct files_lines_s
{
  Dwarf_Off debug_line_offset;
  Dwarf_Files *files;
  Dwarf_Lines *lines;
};

/* Valid indices for the section data.  */
enum
  {
    IDX_debug_info = 0,
    IDX_debug_types,
    IDX_debug_abbrev,
    IDX_debug_aranges,
    IDX_debug_addr,
    IDX_debug_line,
    IDX_debug_line_str,
    IDX_debug_frame,
    IDX_debug_loc,
    IDX_debug_loclists,
    IDX_debug_pubnames,
    IDX_debug_str,
    IDX_debug_str_offsets,
    IDX_debug_macinfo,
    IDX_debug_macro,
    IDX_debug_ranges,
    IDX_debug_rnglists,
    IDX_debug_cu_index,
    IDX_debug_tu_index,
    IDX_gnu_debugaltlink,
    IDX_last
  };

/* Valid indices for the string section's information.  */
enum string_section_index
  {
    STR_SCN_IDX_debug_line_str,
    STR_SCN_IDX_debug_str,
    STR_SCN_IDX_last
  };

/* Error values.  */
enum
{
  DWARF_E_NOERROR = 0,
  DWARF_E_UNKNOWN_ERROR,
  DWARF_E_INVALID_ACCESS,
  DWARF_E_NO_REGFILE,
  DWARF_E_IO_ERROR,
  DWARF_E_INVALID_ELF,
  DWARF_E_NO_DWARF,
  DWARF_E_COMPRESSED_ERROR,
  DWARF_E_NOELF,
  DWARF_E_GETEHDR_ERROR,
  DWARF_E_NOMEM,
  DWARF_E_UNIMPL,
  DWARF_E_INVALID_CMD,
  DWARF_E_INVALID_VERSION,
  DWARF_E_INVALID_FILE,
  DWARF_E_NO_ENTRY,
  DWARF_E_INVALID_DWARF,
  DWARF_E_NO_STRING,
  DWARF_E_NO_DEBUG_STR,
  DWARF_E_NO_DEBUG_LINE_STR,
  DWARF_E_NO_STR_OFFSETS,
  DWARF_E_NO_ADDR,
  DWARF_E_NO_CONSTANT,
  DWARF_E_NO_REFERENCE,
  DWARF_E_INVALID_REFERENCE,
  DWARF_E_NO_DEBUG_LINE,
  DWARF_E_INVALID_DEBUG_LINE,
  DWARF_E_TOO_BIG,
  DWARF_E_VERSION,
  DWARF_E_INVALID_DIR_IDX,
  DWARF_E_ADDR_OUTOFRANGE,
  DWARF_E_NO_DEBUG_LOC,
  DWARF_E_NO_DEBUG_LOCLISTS,
  DWARF_E_NO_LOC_VALUE,
  DWARF_E_NO_BLOCK,
  DWARF_E_INVALID_LINE_IDX,
  DWARF_E_INVALID_ARANGE_IDX,
  DWARF_E_NO_MATCH,
  DWARF_E_NO_FLAG,
  DWARF_E_INVALID_OFFSET,
  DWARF_E_NO_DEBUG_RANGES,
  DWARF_E_NO_DEBUG_RNGLISTS,
  DWARF_E_INVALID_CFI,
  DWARF_E_NO_ALT_DEBUGLINK,
  DWARF_E_INVALID_OPCODE,
  DWARF_E_NOT_CUDIE,
  DWARF_E_UNKNOWN_LANGUAGE,
  DWARF_E_NO_DEBUG_ADDR,
  DWARF_E_UNKNOWN_SECTION,
};


#include "dwarf_sig8_hash.h"

/* The type of Dwarf object, sorted by preference
   (if there is a higher order type, we pick that one over the others).  */
enum dwarf_type
  {
    TYPE_UNKNOWN = 0,
    TYPE_GNU_LTO = 16,
    TYPE_DWO = 32,
    TYPE_PLAIN = 64,
  };

/* This is the structure representing the debugging state.  */
struct Dwarf
{
  /* The underlying ELF file.  */
  Elf *elf;

  /* The (absolute) path to the ELF file, if known.  To help locating
     dwp files.  */
  char *elfpath;

  /* The (absolute) path to the ELF dir, if known.  To help locating
     alt and dwo files.  */
  char *debugdir;

  /* dwz alternate DWARF file.  */
  Dwarf *alt_dwarf;

  /* DWARF package file.  */
  Dwarf *dwp_dwarf;

  /* The section data.  */
  Elf_Data *sectiondata[IDX_last];

  /* Size of a prefix of string sections, where any string will be
     null-terminated. */
  size_t string_section_size[STR_SCN_IDX_last];

  /* True if the file has a byte order different from the host.  */
  bool other_byte_order;

  /* If true, we allocated the ELF descriptor ourselves.  */
  bool free_elf;

  /* If >= 0, we allocated the alt_dwarf ourselves and must end it and
     close this file descriptor.  */
  int alt_fd;

  /* File descriptor of DWARF package file.  */
  int dwp_fd;

  /* Information for traversing the .debug_pubnames section.  This is
     an array and separately allocated with malloc.  */
  struct pubnames_s
  {
    Dwarf_Off cu_offset;
    Dwarf_Off set_start;
    unsigned int cu_header_size;
    int address_len;
  } *pubnames_sets;
  size_t pubnames_nsets;

  /* Search tree for the CUs.  */
  void *cu_tree;
  Dwarf_Off next_cu_offset;

  /* Search tree and sig8 hash table for .debug_types type units.  */
  void *tu_tree;
  Dwarf_Off next_tu_offset;
  Dwarf_Sig8_Hash sig8_hash;

  /* Search tree for split Dwarf associated with CUs in this debug.  */
  void *split_tree;

  /* Search tree for .debug_macro operator tables.  */
  void *macro_ops;

  /* Search tree for decoded .debug_line units.  */
  void *files_lines;

  /* Address ranges read from .debug_aranges.  */
  Dwarf_Aranges *aranges;

  /* Address ranges inferred from CUs.  */
  Dwarf_Aranges *dieranges;

  /* Cached info from the CFI section.  */
  struct Dwarf_CFI_s *cfi;

  /* DWARF package file CU index section.  */
  struct Dwarf_Package_Index_s *cu_index;
  /* DWARF package file TU index section.  */
  struct Dwarf_Package_Index_s *tu_index;

  /* Fake loc CU.  Used when synthesizing attributes for Dwarf_Ops that
     came from a location list entry in dwarf_getlocation_attr.
     Depending on version this is the .debug_loc or .debug_loclists
     section (could be both if mixing CUs with different DWARF versions).  */
  struct Dwarf_CU *fake_loc_cu;
  struct Dwarf_CU *fake_loclists_cu;

  /* Similar for addrx/constx, which will come from .debug_addr section.  */
  struct Dwarf_CU *fake_addr_cu;

  enum dwarf_type type;

  /* Supporting lock for internal memory handling.  Ensures threads that have
     an entry in the mem_tails array are not disturbed by new threads doing
     allocations for this Dwarf.  */
  pthread_rwlock_t mem_rwl;

  /* Internal memory handling.  This is basically a simplified thread-local
     reimplementation of obstacks.  Unfortunately the standard obstack
     implementation is not usable in libraries.  */
  size_t mem_stacks;
  struct libdw_memblock
  {
    size_t size;
    size_t remaining;
    struct libdw_memblock *prev;
    char mem[0];
  } **mem_tails;

  /* Default size of allocated memory blocks.  */
  size_t mem_default_size;

  /* Registered OOM handler.  */
  Dwarf_OOM oom_handler;
};


/* Abbreviation representation.  */
struct Dwarf_Abbrev
{
  Dwarf_Off offset;	  /* Offset to start of abbrev into .debug_abbrev.  */
  unsigned char *attrp;   /* Pointer to start of attribute name/form pairs. */
  bool has_children : 1;  /* Whether or not the DIE has children. */
  unsigned int code : 31; /* The (unique) abbrev code.  */
  unsigned int tag;	  /* The tag of the DIE. */
} attribute_packed;

#include "dwarf_abbrev_hash.h"


/* Files in line information records.  */
struct Dwarf_Files_s
  {
    unsigned int ndirs;
    unsigned int nfiles;
    struct Dwarf_Fileinfo_s
    {
      char *name;
      Dwarf_Word mtime;
      Dwarf_Word length;
    } info[0];
    /* nfiles of those, followed by char *[ndirs].  */
  };
typedef struct Dwarf_Fileinfo_s Dwarf_Fileinfo;


/* Representation of a row in the line table.  */

struct Dwarf_Line_s
{
  Dwarf_Files *files;

  Dwarf_Addr addr;
  unsigned int file;
  int line;
  unsigned short int column;
  unsigned int is_stmt:1;
  unsigned int basic_block:1;
  unsigned int end_sequence:1;
  unsigned int prologue_end:1;
  unsigned int epilogue_begin:1;
  /* The remaining bit fields are not flags, but hold values presumed to be
     small.  All the flags and other bit fields should add up to 48 bits
     to give the whole struct a nice round size.  */
  unsigned int op_index:8;
  unsigned int isa:8;
  unsigned int discriminator:24;
  /* These are currently only used for the NVIDIA extensions.  */
  unsigned int context;
  unsigned int function_name;
};

struct Dwarf_Lines_s
{
  size_t nlines;
  struct Dwarf_Line_s info[0];
};

/* Representation of address ranges.  */
struct Dwarf_Aranges_s
{
  Dwarf *dbg;
  size_t naranges;

  struct Dwarf_Arange_s
  {
    Dwarf_Addr addr;
    Dwarf_Word length;
    Dwarf_Off offset;
  } info[0];
};

/* DWARF package file unit index.  */
typedef struct Dwarf_Package_Index_s
{
  Dwarf *dbg;
  uint32_t section_count;
  uint32_t unit_count;
  uint32_t slot_count;
  /* Mapping from DW_SECT_* - 1 to column number in the section tables, or
     UINT32_MAX if not present.  */
  uint32_t sections[DW_SECT_RNGLISTS];
  /* Row number of last unit found in the index.  */
  uint32_t last_unit_found;
  const unsigned char *hash_table;
  const unsigned char *indices;
  const unsigned char *section_offsets;
  const unsigned char *section_sizes;
  /* If DW_SECT_INFO section offsets were truncated to 32 bits, recovered
     64-bit offsets.  */
  Dwarf_Off *debug_info_offsets;
} Dwarf_Package_Index;

/* CU representation.  */
struct Dwarf_CU
{
  Dwarf *dbg;
  Dwarf_Off start;
  Dwarf_Off end;
  /* Row number of this unit in DWARF package file index.  */
  uint32_t dwp_row;
  uint8_t address_size;
  uint8_t offset_size;
  uint16_t version;

  size_t sec_idx; /* Normally .debug_info, could be .debug_type or "fake". */

  /* The unit type if version >= 5.  Otherwise 0 for normal CUs (from
     .debug_info) or 1 for v4 type units (from .debug_types).  */
  uint8_t unit_type;

  /* Zero if the unit type doesn't support a die/type offset and/or id/sig.
     Nonzero if it is a v4 type unit or for DWARFv5 units depending on
     unit_type.  */
  size_t subdie_offset;
  uint64_t unit_id8;

  /* If this is a skeleton unit this points to the split compile unit.
     Or the other way around if this is a split compile unit.  Set to -1
     if not yet searched.  Always use __libdw_find_split_unit to access
     this field.  */
  struct Dwarf_CU *split;

  /* Hash table for the abbreviations.  */
  Dwarf_Abbrev_Hash abbrev_hash;
  /* Offset of the first abbreviation.  */
  size_t orig_abbrev_offset;
  /* Offset past last read abbreviation.  */
  size_t last_abbrev_offset;

  /* The srcline information.  */
  Dwarf_Lines *lines;

  /* The source file information.  */
  Dwarf_Files *files;

  /* Known location lists.  */
  void *locs;

  /* Base address for use with ranges and locs.
     Don't access directly, call __libdw_cu_base_address.  */
  Dwarf_Addr base_address;

  /* The offset into the .debug_addr section where index zero begins.
     Don't access directly, call __libdw_cu_addr_base.  */
  Dwarf_Off addr_base;

  /* The offset into the .debug_str_offsets section where index zero begins.
     Don't access directly, call __libdw_cu_str_off_base.  */
  Dwarf_Off str_off_base;

  /* The offset into the .debug_ranges section to use for GNU
     DebugFission split units.  Don't access directly, call
     __libdw_cu_ranges_base.  */
  Dwarf_Off ranges_base;

  /* The start of the offset table in .debug_loclists.
     Don't access directly, call __libdw_cu_locs_base.  */
  Dwarf_Off locs_base;

  /* Memory boundaries of this CU.  */
  void *startp;
  void *endp;
};

/* Aliases to avoid PLTs.  */
INTDECL (dwarf_aggregate_size)
INTDECL (dwarf_attr)
INTDECL (dwarf_attr_integrate)
INTDECL (dwarf_begin)
INTDECL (dwarf_begin_elf)
INTDECL (dwarf_child)
INTDECL (dwarf_cu_dwp_section_info)
INTDECL (dwarf_default_lower_bound)
INTDECL (dwarf_dieoffset)
INTDECL (dwarf_diename)
INTDECL (dwarf_end)
INTDECL (dwarf_entrypc)
INTDECL (dwarf_errmsg)
INTDECL (dwarf_formaddr)
INTDECL (dwarf_formblock)
INTDECL (dwarf_formref_die)
INTDECL (dwarf_formsdata)
INTDECL (dwarf_formstring)
INTDECL (dwarf_formudata)
INTDECL (dwarf_getabbrevattr_data)
INTDECL (dwarf_getalt)
INTDECL (dwarf_getarange_addr)
INTDECL (dwarf_getarangeinfo)
INTDECL (dwarf_getaranges)
INTDECL (dwarf_getlocation_die)
INTDECL (dwarf_getsrcfiles)
INTDECL (dwarf_getsrclines)
INTDECL (dwarf_get_units)
INTDECL (dwarf_hasattr)
INTDECL (dwarf_haschildren)
INTDECL (dwarf_haspc)
INTDECL (dwarf_highpc)
INTDECL (dwarf_lowpc)
INTDECL (dwarf_nextcu)
INTDECL (dwarf_next_unit)
INTDECL (dwarf_offdie)
INTDECL (dwarf_peel_type)
INTDECL (dwarf_ranges)
INTDECL (dwarf_setalt)
INTDECL (dwarf_siblingof)
INTDECL (dwarf_srclang)
INTDECL (dwarf_tag)

#define ISV4TU(cu) ((cu)->version == 4 && (cu)->sec_idx == IDX_debug_types)

/* Compute the offset of a CU's first DIE from the CU offset.
   CU must be a valid/known version/unit_type.  */
static inline Dwarf_Off
__libdw_first_die_from_cu_start (Dwarf_Off cu_start,
				 uint8_t offset_size,
				 uint16_t version,
				 uint8_t unit_type)
{
/*
  assert (offset_size == 4 || offset_size == 8);
  assert (version >= 2 && version <= 5);
  assert (unit_type == DW_UT_compile
	  || unit_type == DW_UT_partial
	  || unit_type == DW_UT_skeleton
	  || unit_type == DW_UT_split_compile
	  || unit_type == DW_UT_type
	  || unit_type == DW_UT_split_type);
*/

  Dwarf_Off off = cu_start;
  if (version < 5)
    {
   /*
        LEN       VER     OFFSET    ADDR
      4-bytes + 2-bytes + 4-bytes + 1-byte  for 32-bit dwarf
     12-bytes + 2-bytes + 8-bytes + 1-byte  for 64-bit dwarf
   or in .debug_types, 			     SIGNATURE TYPE-OFFSET
      4-bytes + 2-bytes + 4-bytes + 1-byte + 8-bytes + 4-bytes  for 32-bit
     12-bytes + 2-bytes + 8-bytes + 1-byte + 8-bytes + 8-bytes  for 64-bit

   Note the trick in the computation.  If the offset_size is 4
   the '- 4' term changes the '3 *' (or '4 *') into a '2 *' (or '3 *).
   If the offset_size is 8 it accounts for the 4-byte escape value
   used at the start of the length.  */
      if (unit_type != DW_UT_type)
	off += 3 * offset_size - 4 + 3;
      else
	off += 4 * offset_size - 4 + 3 + 8;
    }
  else
    {
     /*
        LEN       VER      TYPE     ADDR     OFFSET   SIGNATURE  TYPE-OFFSET
      4-bytes + 2-bytes + 1-byte + 1-byte + 4-bytes + 8-bytes + 4-bytes 32-bit
     12-bytes + 2-bytes + 1-byte + 1-byte + 8-bytes + 8-bytes + 8-bytes 64-bit
        Both signature and type offset are optional.

        Note same 4/8 offset size trick as above.
        We explicitly ignore unknown unit types (see asserts above).  */
      off += 3 * offset_size - 4 + 4;
      if (unit_type == DW_UT_skeleton || unit_type == DW_UT_split_compile
	  || unit_type == DW_UT_type || unit_type == DW_UT_split_type)
	{
	  off += 8;
	  if (unit_type == DW_UT_type || unit_type == DW_UT_split_type)
	    off += offset_size;
	}
    }

  return off;
}

static inline Dwarf_Off
__libdw_first_die_off_from_cu (struct Dwarf_CU *cu)
{
  return __libdw_first_die_from_cu_start (cu->start,
					  cu->offset_size,
					  cu->version,
					  cu->unit_type);
}

#define CUDIE(fromcu)							      \
  ((Dwarf_Die)								      \
   {									      \
     .cu = (fromcu),							      \
     .addr = ((char *) (fromcu)->dbg->sectiondata[cu_sec_idx (fromcu)]->d_buf \
	      + __libdw_first_die_off_from_cu (fromcu))			      \
   })

#define SUBDIE(fromcu)							      \
  ((Dwarf_Die)								      \
   {									      \
     .cu = (fromcu),							      \
     .addr = ((char *) (fromcu)->dbg->sectiondata[cu_sec_idx (fromcu)]->d_buf \
	      + (fromcu)->start + (fromcu)->subdie_offset)		      \
   })


/* Prototype of a single .debug_macro operator.  */
typedef struct
{
  Dwarf_Word nforms;
  unsigned char const *forms;
} Dwarf_Macro_Op_Proto;

/* Prototype table.  */
typedef struct
{
  Dwarf *dbg;

  /* Offset of .debug_macro section.  */
  Dwarf_Off offset;

  /* Offset of associated .debug_line section.  */
  Dwarf_Off line_offset;

  /* The source file information.  */
  Dwarf_Files *files;

  /* If this macro unit was opened through dwarf_getmacros or
     dwarf_getmacros_die, this caches value of DW_AT_comp_dir, if
     present.  */
  const char *comp_dir;

  /* Header length.  */
  Dwarf_Half header_len;

  uint16_t version;
  uint8_t address_size;
  uint8_t offset_size;
  uint8_t sec_index;	/* IDX_debug_macro or IDX_debug_macinfo.  */

  /* Shows where in TABLE each opcode is defined.  Since opcode 0 is
     never used, it stores index of opcode X in X-1'th element.  The
     value of 0xff means not stored at all.  */
  unsigned char opcodes[255];

  /* Individual opcode prototypes.  */
  Dwarf_Macro_Op_Proto table[];
} Dwarf_Macro_Op_Table;

struct Dwarf_Macro_s
{
  Dwarf_Macro_Op_Table *table;
  Dwarf_Attribute *attributes;
  uint8_t opcode;
};

static inline Dwarf_Word
libdw_macro_nforms (Dwarf_Macro *macro)
{
  return macro->table->table[macro->table->opcodes[macro->opcode - 1]].nforms;
}

/* Returns true for any allowed FORM in the opcode_operands_table as
   mentioned in the DWARF5 spec (6.3.1 Macro Information Header).
   Or those mentioned in DWARF5 spec (6.2.4.2 Vendor-defined Content
   Descriptions) for the directory/file table (plus DW_FORM_strp_sup).  */
static inline bool
libdw_valid_user_form (int form)
{
  switch (form)
    {
      case DW_FORM_block:
      case DW_FORM_block1:
      case DW_FORM_block2:
      case DW_FORM_block4:
      case DW_FORM_data1:
      case DW_FORM_data2:
      case DW_FORM_data4:
      case DW_FORM_data8:
      case DW_FORM_data16:
      case DW_FORM_flag:
      case DW_FORM_line_strp:
      case DW_FORM_sdata:
      case DW_FORM_sec_offset:
      case DW_FORM_string:
      case DW_FORM_strp:
      case DW_FORM_strp_sup:
      case DW_FORM_strx:
      case DW_FORM_strx1:
      case DW_FORM_strx2:
      case DW_FORM_strx3:
      case DW_FORM_strx4:
      case DW_FORM_udata:
	return true;
      default:
	return false;
    }
}


/* We have to include the file at this point because the inline
   functions access internals of the Dwarf structure.  */
#include "memory-access.h"


/* Set error value.  */
extern void __libdw_seterrno (int value) internal_function;


/* Memory handling, the easy parts.  */
#define libdw_alloc(dbg, type, tsize, cnt) \
  ({ struct libdw_memblock *_tail = __libdw_alloc_tail(dbg);		      \
     size_t _required = (tsize) * (cnt);				      \
     type *_result = (type *) (_tail->mem + (_tail->size - _tail->remaining));\
     size_t _padding = ((__alignof (type)				      \
			 - ((uintptr_t) _result & (__alignof (type) - 1)))    \
			& (__alignof (type) - 1));			      \
     if (unlikely (_tail->remaining < _required + _padding))		      \
       _result = (type *) __libdw_allocate (dbg, _required, __alignof (type));\
     else								      \
       {								      \
	 _required += _padding;						      \
	 _result = (type *) ((char *) _result + _padding);		      \
	 _tail->remaining -= _required;					      \
       }								      \
     _result; })

#define libdw_typed_alloc(dbg, type) \
  libdw_alloc (dbg, type, sizeof (type), 1)

/* Can only be used to undo the last libdw_alloc.  */
#define libdw_unalloc(dbg, type, tsize, cnt) \
  ({ struct libdw_memblock *_tail = __libdw_thread_tail (dbg);		      \
     size_t _required = (tsize) * (cnt);				      \
     /* We cannot know the padding, it is lost.  */			      \
     _tail->remaining += _required; })					      \

#define libdw_typed_unalloc(dbg, type) \
  libdw_unalloc (dbg, type, sizeof (type), 1)

/* Callback to choose a thread-local memory allocation stack.  */
extern struct libdw_memblock *__libdw_alloc_tail (Dwarf* dbg)
     __nonnull_attribute__ (1);

extern struct libdw_memblock *__libdw_thread_tail (Dwarf* dbg)
     __nonnull_attribute__ (1);

/* Callback to allocate more.  */
extern void *__libdw_allocate (Dwarf *dbg, size_t minsize, size_t align)
     __attribute__ ((__malloc__)) __nonnull_attribute__ (1);

/* Default OOM handler.  */
extern void __libdw_oom (void) __attribute ((noreturn)) attribute_hidden;

/* Read next unit (or v4 debug type) and return next offset.  Doesn't
   create an actual Dwarf_CU just provides necessary header fields.  */
extern int
internal_function
__libdw_next_unit (Dwarf *dbg, bool v4_debug_types, Dwarf_Off off,
		   Dwarf_Off *next_off, size_t *header_sizep,
		   Dwarf_Half *versionp, uint8_t *unit_typep,
		   Dwarf_Off *abbrev_offsetp, uint8_t *address_sizep,
		   uint8_t *offset_sizep, uint64_t *unit_id8p,
		   Dwarf_Off *subdie_offsetp)
     __nonnull_attribute__ (4) internal_function;

/* Allocate the internal data for a unit not seen before.  */
extern struct Dwarf_CU *__libdw_intern_next_unit (Dwarf *dbg, bool debug_types)
     __nonnull_attribute__ (1) internal_function;

/* Find CU for given offset.  */
extern struct Dwarf_CU *__libdw_findcu (Dwarf *dbg, Dwarf_Off offset, bool tu)
     __nonnull_attribute__ (1) internal_function;

/* Find CU for given DIE address.  */
extern struct Dwarf_CU *__libdw_findcu_addr (Dwarf *dbg, void *addr)
     __nonnull_attribute__ (1) internal_function;

/* Find split Dwarf for given DIE address.  */
extern struct Dwarf *__libdw_find_split_dbg_addr (Dwarf *dbg, void *addr)
     __nonnull_attribute__ (1) internal_function;

/* Find the split (or skeleton) unit.  */
extern struct Dwarf_CU *__libdw_find_split_unit (Dwarf_CU *cu)
     internal_function;

/* Find a unit in a DWARF package file for __libdw_intern_next_unit.  */
extern int __libdw_dwp_find_unit (Dwarf *dbg, bool debug_types, Dwarf_Off off,
				  uint16_t version, uint8_t unit_type,
				  uint64_t unit_id8, uint32_t *unit_rowp,
				  Dwarf_Off *abbrev_offsetp)
     __nonnull_attribute__ (1, 7, 8) internal_function;

/* Find the compilation unit in a DWARF package file with the given id.  */
extern Dwarf_CU *__libdw_dwp_findcu_id (Dwarf *dbg, uint64_t unit_id8)
     __nonnull_attribute__ (1) internal_function;

/* Get abbreviation with given code.  */
extern Dwarf_Abbrev *__libdw_findabbrev (struct Dwarf_CU *cu,
					 unsigned int code)
     __nonnull_attribute__ (1) internal_function;

/* Get abbreviation at given offset.  */
extern Dwarf_Abbrev *__libdw_getabbrev (Dwarf *dbg, struct Dwarf_CU *cu,
					Dwarf_Off offset, size_t *lengthp,
					Dwarf_Abbrev *result)
     __nonnull_attribute__ (1) internal_function;

/* Get abbreviation of given DIE, and optionally set *READP to the DIE memory
   just past the abbreviation code.  */
static inline Dwarf_Abbrev *
__nonnull_attribute__ (1)
__libdw_dieabbrev (Dwarf_Die *die, const unsigned char **readp)
{
  /* Do we need to get the abbreviation, or need to read after the code?  */
  if (die->abbrev == NULL || readp != NULL)
    {
      /* Get the abbreviation code.  */
      unsigned int code;
      const unsigned char *addr = die->addr;
      if (unlikely (die->cu == NULL
		    || addr >= (const unsigned char *) die->cu->endp))
	return die->abbrev = DWARF_END_ABBREV;
      get_uleb128 (code, addr, die->cu->endp);
      if (readp != NULL)
	*readp = addr;

      /* Find the abbreviation.  */
      if (die->abbrev == NULL)
	die->abbrev = __libdw_findabbrev (die->cu, code);
    }
  return die->abbrev;
}

/* Helper functions for form handling.  */
extern size_t __libdw_form_val_compute_len (struct Dwarf_CU *cu,
					    unsigned int form,
					    const unsigned char *valp)
     __nonnull_attribute__ (1, 3) internal_function;

/* Find the length of a form attribute in DIE/info data.  */
static inline size_t
__nonnull_attribute__ (1, 3)
__libdw_form_val_len (struct Dwarf_CU *cu, unsigned int form,
		      const unsigned char *valp)
{
  /* Small lookup table of forms with fixed lengths.  Absent indexes are
     initialized 0, so any truly desired 0 is set to 0x80 and masked.  */
  static const uint8_t form_lengths[] =
    {
      [DW_FORM_flag_present] = 0x80,
      [DW_FORM_implicit_const] = 0x80, /* Value is in abbrev, not in info.  */

      [DW_FORM_flag] = 1,
      [DW_FORM_data1] = 1, [DW_FORM_ref1] = 1,
      [DW_FORM_addrx1] = 1, [DW_FORM_strx1] = 1,

      [DW_FORM_data2] = 2, [DW_FORM_ref2] = 2,
      [DW_FORM_addrx2] = 2, [DW_FORM_strx2] = 2,

      [DW_FORM_addrx3] = 3, [DW_FORM_strx3] = 3,

      [DW_FORM_data4] = 4, [DW_FORM_ref4] = 4, [DW_FORM_ref_sup4] = 4,
      [DW_FORM_addrx4] = 4, [DW_FORM_strx4] = 4,

      [DW_FORM_ref_sig8] = 8,
      [DW_FORM_data8] = 8, [DW_FORM_ref8] = 8, [DW_FORM_ref_sup8] = 8,

      [DW_FORM_data16] = 16,
    };

  /* Return immediately for forms with fixed lengths.  */
  if (form < sizeof form_lengths / sizeof form_lengths[0])
    {
      uint8_t len = form_lengths[form];
      if (len != 0)
	{
	  const unsigned char *endp = cu->endp;
	  len &= 0x7f; /* Mask to allow 0x80 -> 0.  */
	  if (unlikely (len > (size_t) (endp - valp)))
	    {
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return -1;
	    }
	  return len;
	}
    }

  /* Other forms require some computation.  */
  return __libdw_form_val_compute_len (cu, form, valp);
}

/* Helper function for DW_FORM_ref* handling.  */
extern int __libdw_formref (Dwarf_Attribute *attr, Dwarf_Off *return_offset)
     __nonnull_attribute__ (1, 2) internal_function;


/* Helper function to locate attribute.  */
extern unsigned char *__libdw_find_attr (Dwarf_Die *die,
					 unsigned int search_name,
					 unsigned int *codep,
					 unsigned int *formp)
     __nonnull_attribute__ (1) internal_function;

/* Helper function to access integer attribute.  */
extern int __libdw_attr_intval (Dwarf_Die *die, int *valp, int attval)
     __nonnull_attribute__ (1, 2) internal_function;

/* Helper function to walk scopes.  */
struct Dwarf_Die_Chain
{
  Dwarf_Die die;
  struct Dwarf_Die_Chain *parent;
  bool prune;			/* The PREVISIT function can set this.  */
};
extern int __libdw_visit_scopes (unsigned int depth,
				 struct Dwarf_Die_Chain *root,
				 struct Dwarf_Die_Chain *imports,
				 int (*previsit) (unsigned int depth,
						  struct Dwarf_Die_Chain *,
						  void *arg),
				 int (*postvisit) (unsigned int depth,
						   struct Dwarf_Die_Chain *,
						   void *arg),
				 void *arg)
  __nonnull_attribute__ (2, 4) internal_function;

/* Parse a DWARF Dwarf_Block into an array of Dwarf_Op's,
   and cache the result (via tsearch).  */
extern int __libdw_intern_expression (Dwarf *dbg,
				      bool other_byte_order,
				      unsigned int address_size,
				      unsigned int ref_size,
				      void **cache, const Dwarf_Block *block,
				      bool cfap, bool valuep,
				      Dwarf_Op **llbuf, size_t *listlen,
				      int sec_index)
  __nonnull_attribute__ (5, 6, 9, 10) internal_function;

extern Dwarf_Die *__libdw_offdie (Dwarf *dbg, Dwarf_Off offset,
				  Dwarf_Die *result, bool debug_types)
  internal_function;


/* Return error code of last failing function call.  This value is kept
   separately for each thread.  */
extern int __dwarf_errno_internal (void);


/* Reader hooks.  */

/* Relocation hooks return -1 on error (in that case the error code
   must already have been set), 0 if there is no relocation and 1 if a
   relocation was present.*/

static inline int
__libdw_relocate_address (Dwarf *dbg __attribute__ ((unused)),
			  int sec_index __attribute__ ((unused)),
			  const void *addr __attribute__ ((unused)),
			  int width __attribute__ ((unused)),
			  Dwarf_Addr *val __attribute__ ((unused)))
{
  return 0;
}

static inline int
__libdw_relocate_offset (Dwarf *dbg __attribute__ ((unused)),
			 int sec_index __attribute__ ((unused)),
			 const void *addr __attribute__ ((unused)),
			 int width __attribute__ ((unused)),
			 Dwarf_Off *val __attribute__ ((unused)))
{
  return 0;
}

static inline Elf_Data *
__libdw_checked_get_data (Dwarf *dbg, int sec_index)
{
  Elf_Data *data = dbg->sectiondata[sec_index];
  if (unlikely (data == NULL)
      || unlikely (data->d_buf == NULL))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }
  return data;
}

static inline int
__libdw_offset_in_section (Dwarf *dbg, int sec_index,
			   Dwarf_Off offset, size_t size)
{
  Elf_Data *data = __libdw_checked_get_data (dbg, sec_index);
  if (data == NULL)
    return -1;
  if (unlikely (offset > data->d_size)
      || unlikely (data->d_size < size)
      || unlikely (offset > data->d_size - size))
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return -1;
    }

  return 0;
}

static inline bool
__libdw_in_section (Dwarf *dbg, int sec_index,
		    const void *addr, size_t size)
{
  Elf_Data *data = __libdw_checked_get_data (dbg, sec_index);
  if (data == NULL)
    return false;
  if (unlikely (addr < data->d_buf)
      || unlikely (data->d_size < size)
      || unlikely ((size_t)(addr - data->d_buf) > data->d_size - size))
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return false;
    }

  return true;
}

#define READ_AND_RELOCATE(RELOC_HOOK, VAL)				\
  ({									\
    if (!__libdw_in_section (dbg, sec_index, addr, width))		\
      return -1;							\
									\
    const unsigned char *orig_addr = addr;				\
    if (width == 4)							\
      VAL = read_4ubyte_unaligned_inc (dbg, addr);			\
    else								\
      VAL = read_8ubyte_unaligned_inc (dbg, addr);			\
									\
    int status = RELOC_HOOK (dbg, sec_index, orig_addr, width, &VAL);	\
    if (status < 0)							\
      return status;							\
    status > 0;								\
   })

static inline int
__libdw_read_address_inc (Dwarf *dbg,
			  int sec_index, const unsigned char **addrp,
			  int width, Dwarf_Addr *ret)
{
  const unsigned char *addr = *addrp;
  READ_AND_RELOCATE (__libdw_relocate_address, (*ret));
  *addrp = addr;
  return 0;
}

static inline int
__libdw_read_address (Dwarf *dbg,
		      int sec_index, const unsigned char *addr,
		      int width, Dwarf_Addr *ret)
{
  READ_AND_RELOCATE (__libdw_relocate_address, (*ret));
  return 0;
}

static inline int
__libdw_read_offset_inc (Dwarf *dbg,
			 int sec_index, const unsigned char **addrp,
			 int width, Dwarf_Off *ret, int sec_ret,
			 size_t size)
{
  const unsigned char *addr = *addrp;
  READ_AND_RELOCATE (__libdw_relocate_offset, (*ret));
  *addrp = addr;
  return __libdw_offset_in_section (dbg, sec_ret, *ret, size);
}

static inline int
__libdw_read_offset (Dwarf *dbg, Dwarf *dbg_ret,
		     int sec_index, const unsigned char *addr,
		     int width, Dwarf_Off *ret, int sec_ret,
		     size_t size)
{
  READ_AND_RELOCATE (__libdw_relocate_offset, (*ret));
  return __libdw_offset_in_section (dbg_ret, sec_ret, *ret, size);
}

static inline size_t
cu_sec_idx (struct Dwarf_CU *cu)
{
  return cu->sec_idx;
}

static inline bool
is_cudie (Dwarf_Die *cudie)
{
  return cudie->cu != NULL && CUDIE (cudie->cu).addr == cudie->addr;
}

/* Read up begin/end pair and increment read pointer.
    - If it's normal range record, set up *BEGINP and *ENDP and return 0.
    - If it's base address selection record, set up *BASEP and return 1.
    - If it's end of rangelist, don't set anything and return 2
    - If an error occurs, don't set anything and return <0.  */
int __libdw_read_begin_end_pair_inc (Dwarf_CU *cu, int sec_index,
				     const unsigned char **readp,
				     const unsigned char *readend,
				     int width,
				     Dwarf_Addr *beginp, Dwarf_Addr *endp,
				     Dwarf_Addr *basep)
  internal_function;

const unsigned char * __libdw_formptr (Dwarf_Attribute *attr, int sec_index,
				       int err_nodata,
				       const unsigned char **endpp,
				       Dwarf_Off *offsetp)
  internal_function;

/* Fills in the given attribute to point at an empty location expression.  */
void __libdw_empty_loc_attr (Dwarf_Attribute *attr)
  internal_function;

/* Load .debug_line unit at DEBUG_LINE_OFFSET.  COMP_DIR is a value of
   DW_AT_comp_dir or NULL if that attribute is not available.  Caches
   the loaded unit and optionally set *LINESP and/or *FILESP (if not
   NULL) with loaded information.  Returns 0 for success or a negative
   value for failure.  */
int __libdw_getsrclines (Dwarf *dbg, Dwarf_Off debug_line_offset,
			 const char *comp_dir, unsigned address_size,
			 Dwarf_Lines **linesp, Dwarf_Files **filesp)
  internal_function
  __nonnull_attribute__ (1);

/* Load and return value of DW_AT_comp_dir from CUDIE.  */
const char *__libdw_getcompdir (Dwarf_Die *cudie);

/* Get the base address for the CU, fetches it when not yet set.
   This is used as initial base address for ranges and loclists.  */
Dwarf_Addr __libdw_cu_base_address (Dwarf_CU *cu);

/* Get the address base for the CU, fetches it when not yet set.  */
static inline Dwarf_Off
__libdw_cu_addr_base (Dwarf_CU *cu)
{
  if (cu->addr_base == (Dwarf_Off) -1)
    {
      Dwarf_Die cu_die = CUDIE(cu);
      Dwarf_Attribute attr;
      Dwarf_Off offset = 0;
      if (dwarf_attr (&cu_die, DW_AT_GNU_addr_base, &attr) != NULL
	  || dwarf_attr (&cu_die, DW_AT_addr_base, &attr) != NULL)
	{
	  Dwarf_Word off;
	  if (dwarf_formudata (&attr, &off) == 0)
	    offset = off;
	}
      cu->addr_base = offset;
    }

  return cu->addr_base;
}

/* Gets the .debug_str_offsets base offset to use.  static inline to
   be shared between libdw and eu-readelf.  */
static inline Dwarf_Off
str_offsets_base_off (Dwarf *dbg, Dwarf_CU *cu)
{
  /* If we don't have a CU, then find and use the first one in the
     debug file (when we support .dwp files, we must actually find the
     one matching our "caller" - aka macro or line).  If we (now) have
     a cu and str_offsets_base attribute, just use that.  Otherwise
     use the first offset.  But we might have to parse the header
     first, but only if this is version 5.  Assume if all else fails,
     this is version 4, without header.  */

  if (cu == NULL && dbg != NULL)
    {
      Dwarf_CU *first_cu;
      if (INTUSE(dwarf_get_units) (dbg, NULL, &first_cu,
				   NULL, NULL, NULL, NULL) == 0)
	cu = first_cu;
    }

  Dwarf_Off off = 0;
  if (cu != NULL)
    {
      if (cu->str_off_base == (Dwarf_Off) -1)
	{
	  Dwarf_Off dwp_offset;
	  if (dwarf_cu_dwp_section_info (cu, DW_SECT_STR_OFFSETS, &dwp_offset,
					 NULL) == 0)
	    off = dwp_offset;
	  Dwarf_Die cu_die = CUDIE(cu);
	  Dwarf_Attribute attr;
	  if (dwarf_attr (&cu_die, DW_AT_str_offsets_base, &attr) != NULL)
	    {
	      Dwarf_Word base;
	      if (dwarf_formudata (&attr, &base) == 0)
		{
		  cu->str_off_base = off + base;
		  return cu->str_off_base;
		}
	    }
	  /* For older DWARF simply assume zero (no header).  */
	  if (cu->version < 5)
	    {
	      cu->str_off_base = off;
	      return cu->str_off_base;
	    }

	  if (dbg == NULL)
	    dbg = cu->dbg;
	}
      else
	return cu->str_off_base;
    }

  /* No str_offsets_base attribute, we have to assume "zero".
     But there could be a header first.  */
  if (dbg == NULL)
    goto no_header;

  Elf_Data *data =  dbg->sectiondata[IDX_debug_str_offsets];
  if (data == NULL)
    goto no_header;

  const unsigned char *start;
  const unsigned char *readp;
  const unsigned char *readendp;
  start = readp = (const unsigned char *) data->d_buf;
  readendp = (const unsigned char *) data->d_buf + data->d_size;

  uint64_t unit_length;
  uint16_t version;

  unit_length = read_4ubyte_unaligned_inc (dbg, readp);
  if (unlikely (unit_length == 0xffffffff))
    {
      if (unlikely (readendp - readp < 8))
	goto no_header;
      unit_length = read_8ubyte_unaligned_inc (dbg, readp);
      /* In theory the offset size could be different
	 between CU and str_offsets unit.  But we just
	 ignore that here. */
    }

  /* We need at least 2-bytes (version) + 2-bytes (padding) =
     4 bytes to complete the header.  And this unit cannot go
     beyond the section data.  */
  if (readendp - readp < 4
      || unit_length < 4
      || (uint64_t) (readendp - readp) < unit_length)
    goto no_header;

  version = read_2ubyte_unaligned_inc (dbg, readp);
  if (version != 5)
    goto no_header;
  /* padding */
  read_2ubyte_unaligned_inc (dbg, readp);

  off += (Dwarf_Off) (readp - start);

 no_header:
  if (cu != NULL)
    cu->str_off_base = off;

  return off;
}


/* Get the string offsets base for the CU, fetches it when not yet set.  */
static inline Dwarf_Off __libdw_cu_str_off_base (Dwarf_CU *cu)
{
  return str_offsets_base_off (NULL, cu);
}


/* Either a direct offset into .debug_ranges for version < 5, or the
   start of the offset table in .debug_rnglists for version > 5.  */
static inline Dwarf_Off
__libdw_cu_ranges_base (Dwarf_CU *cu)
{
  if (cu->ranges_base == (Dwarf_Off) -1)
    {
      Dwarf_Off offset = 0;
      Dwarf_Die cu_die = CUDIE(cu);
      Dwarf_Attribute attr;
      if (cu->version < 5)
	{
	  if (dwarf_attr (&cu_die, DW_AT_GNU_ranges_base, &attr) != NULL)
	    {
	      Dwarf_Word off;
	      if (dwarf_formudata (&attr, &off) == 0)
		offset = off;
	    }
	}
      else
	{
	  Dwarf_Off dwp_offset;
	  if (dwarf_cu_dwp_section_info (cu, DW_SECT_RNGLISTS, &dwp_offset,
					 NULL) == 0)
	    offset = dwp_offset;

	  if (dwarf_attr (&cu_die, DW_AT_rnglists_base, &attr) != NULL)
	    {
	      Dwarf_Word off;
	      if (dwarf_formudata (&attr, &off) == 0)
		offset += off;
	    }

	  /* There wasn't an rnglists_base, if the Dwarf does have a
	     .debug_rnglists section, then it might be we need the
	     base after the first header. */
	  Elf_Data *data = cu->dbg->sectiondata[IDX_debug_rnglists];
	  if (offset == dwp_offset && data != NULL)
	    {
	      Dwarf *dbg = cu->dbg;
	      const unsigned char *readp = data->d_buf;
	      const unsigned char *const dataend
		= (unsigned char *) data->d_buf + data->d_size;

	      uint64_t unit_length = read_4ubyte_unaligned_inc (dbg, readp);
	      unsigned int offset_size = 4;
	      if (unlikely (unit_length == 0xffffffff))
		{
		  if (unlikely (readp > dataend - 8))
		    goto no_header;

		  unit_length = read_8ubyte_unaligned_inc (dbg, readp);
		  offset_size = 8;
		}

	      if (readp > dataend - 8
		  || unit_length < 8
		  || unit_length > (uint64_t) (dataend - readp))
		goto no_header;

	      uint16_t version = read_2ubyte_unaligned_inc (dbg, readp);
	      if (version != 5)
		goto no_header;

	      uint8_t address_size = *readp++;
	      if (address_size != 4 && address_size != 8)
		goto no_header;

	      uint8_t segment_size = *readp++;
	      if (segment_size != 0)
		goto no_header;

	      uint32_t offset_entry_count;
	      offset_entry_count = read_4ubyte_unaligned_inc (dbg, readp);

	      const unsigned char *offset_array_start = readp;
	      if (offset_entry_count <= 0)
		goto no_header;

	      uint64_t needed = offset_entry_count * offset_size;
	      if (unit_length - 8 < needed)
		goto no_header;

	      offset += (Dwarf_Off) (offset_array_start
				     - (unsigned char *) data->d_buf);
	    }
	}
    no_header:
      cu->ranges_base = offset;
    }

  return cu->ranges_base;
}


/* The start of the offset table in .debug_loclists for DWARF5.  */
static inline Dwarf_Off
__libdw_cu_locs_base (Dwarf_CU *cu)
{
  if (cu->locs_base == (Dwarf_Off) -1)
    {
      Dwarf_Off offset = 0;
      Dwarf_Off dwp_offset;
      if (dwarf_cu_dwp_section_info (cu, DW_SECT_LOCLISTS, &dwp_offset, NULL)
	  == 0)
	offset = dwp_offset;

      Dwarf_Die cu_die = CUDIE(cu);
      Dwarf_Attribute attr;
      if (dwarf_attr (&cu_die, DW_AT_loclists_base, &attr) != NULL)
	{
	  Dwarf_Word off;
	  if (dwarf_formudata (&attr, &off) == 0)
	    offset += off;
	}

      /* There wasn't an loclists_base, if the Dwarf does have a
	 .debug_loclists section, then it might be we need the
	 base after the first header. */
      Elf_Data *data = cu->dbg->sectiondata[IDX_debug_loclists];
      if (offset == dwp_offset && data != NULL)
	{
	  Dwarf *dbg = cu->dbg;
	  const unsigned char *readp = data->d_buf;
	  const unsigned char *const dataend
	    = (unsigned char *) data->d_buf + data->d_size;

	  uint64_t unit_length = read_4ubyte_unaligned_inc (dbg, readp);
	  unsigned int offset_size = 4;
	  if (unlikely (unit_length == 0xffffffff))
	    {
	      if (unlikely (readp > dataend - 8))
		goto no_header;

	      unit_length = read_8ubyte_unaligned_inc (dbg, readp);
	      offset_size = 8;
	    }

	  if (readp > dataend - 8
	      || unit_length < 8
	      || unit_length > (uint64_t) (dataend - readp))
	    goto no_header;

	  uint16_t version = read_2ubyte_unaligned_inc (dbg, readp);
	  if (version != 5)
	    goto no_header;

	  uint8_t address_size = *readp++;
	  if (address_size != 4 && address_size != 8)
	    goto no_header;

	  uint8_t segment_size = *readp++;
	  if (segment_size != 0)
	    goto no_header;

	  uint32_t offset_entry_count;
	  offset_entry_count = read_4ubyte_unaligned_inc (dbg, readp);

	  const unsigned char *offset_array_start = readp;
	  if (offset_entry_count <= 0)
	    goto no_header;

	  uint64_t needed = offset_entry_count * offset_size;
	  if (unit_length - 8 < needed)
	    goto no_header;

	  offset += (Dwarf_Off) (offset_array_start
				 - (unsigned char *) data->d_buf);
	}

    no_header:
      cu->locs_base = offset;
    }

  return cu->locs_base;
}

/* Helper function for tsearch/tfind split_tree Dwarf.  */
int __libdw_finddbg_cb (const void *arg1, const void *arg2);

/* Link skeleton and split compile units.  */
static inline void
__libdw_link_skel_split (Dwarf_CU *skel, Dwarf_CU *split)
{
  skel->split = split;
  split->split = skel;

  /* Get .debug_addr and addr_base greedy.
     We also need it for the fake addr cu.
     This needs to be done for each split unit (one per .dwo file, or multiple
     per .dwp file).  */
  Dwarf *dbg = skel->dbg;
  Dwarf *sdbg = split->dbg;
  if (dbg->sectiondata[IDX_debug_addr] != NULL
      /* If this split file hasn't been linked yet...  */
      && (sdbg->sectiondata[IDX_debug_addr] == NULL
	  /* ... or it was linked to the same skeleton file for another
	     unit...  */
	  || (sdbg->sectiondata[IDX_debug_addr]
	      == dbg->sectiondata[IDX_debug_addr])))
    {
      /* ... then link the address information for this file and unit.  */
      sdbg->sectiondata[IDX_debug_addr]
	= dbg->sectiondata[IDX_debug_addr];
      split->addr_base = __libdw_cu_addr_base (skel);
      sdbg->fake_addr_cu = dbg->fake_addr_cu;
    }
}


/* Given an address index for a CU return the address.
   Returns -1 and sets libdw_errno if an error occurs.  */
int __libdw_addrx (Dwarf_CU *cu, Dwarf_Word idx, Dwarf_Addr *addr);


/* Helper function to set elfpath field in Dwarf, used from dwarf_begin_elf
   and libdwfl process_file.  */
char * __libdw_elfpath (int fd);

/* Helper function to set debugdir field in Dwarf after elfpath field has been
   set.  */
void __libdw_set_debugdir (Dwarf *dbg);


/* Given the directory of a debug file, an absolute or relative dir
   to look in, and file returns a full path.

   If the file is absolute (starts with a /) a copy of file is returned.
   the file isn't absolute, but dir is absolute, then a path that is
   the concatenation of dir and file is returned.  If neither file,
   nor dir is absolute, the path will be constructed using dir (if not
   NULL) and file relative to the debugdir (if valid).

   The debugdir and the dir may be NULL (in which case they aren't used).
   If file is NULL, or no full path can be constructed NULL is returned.

   The caller is responsible for freeing the result if not NULL.  */
char * __libdw_filepath (const char *debugdir, const char *dir,
			 const char *file)
  internal_function;

/* Like dwarf_getaranges but aranges are generated from CU address
   ranges instead of being read from .debug_aranges.

   Returns 0 if successful and updates ARANGES and NARANGES.  Otherwise
   returns -1 and sets libdw_errno.
*/
int __libdw_getdieranges (Dwarf *dbg, Dwarf_Aranges **aranges, size_t *naranges);
#endif	/* libdwP.h */
