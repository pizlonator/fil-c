/* Internal definitions for libasm.
   Copyright (C) 2002, 2004, 2005, 2016 Red Hat, Inc.
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

#ifndef _LIBASMP_H
#define _LIBASMP_H 1

#include <stdio.h>

#include <libasm.h>

#include <system.h>

#include "libebl.h"

#include "libdwelf.h"


/* Known error codes.  */
enum
  {
    ASM_E_NOERROR,
    ASM_E_NOMEM,		/* No more memory.  */
    ASM_E_CANNOT_CREATE,	/* Output file cannot be created.  */
    ASM_E_INVALID,		/* Invalid parameters.  */
    ASM_E_CANNOT_CHMOD,		/* Cannot change mode of output file.  */
    ASM_E_CANNOT_RENAME,	/* Cannot rename output file.  */
    ASM_E_DUPLSYM,		/* Duplicate symbol definition.  */
    ASM_E_LIBELF,		/* Refer to error in libelf.  */
    ASM_E_TYPE,			/* Invalid section type for operation.  */
    ASM_E_IOERROR,		/* Error during output of data.  */
    ASM_E_ENOSUP,		/* No backend support.  */
    ASM_E_NUM			/* Keep this entry as the last.  */
  };


/* Special sections.  */
#define ASM_ABS_SCN ((Elf_Scn *) 1)
#define ASM_COM_SCN ((Elf_Scn *) 2)


/* And the hash table for symbols.  */
#include <symbolhash.h>


/* Descriptor for a section.  */
struct AsmScn
{
  /* The underlying assembler context.  */
  AsmCtx_t *ctx;

  /* Subsection ID.  */
  unsigned int subsection_id;

  /* Section type.  */
  GElf_Word type;

  union
  {
    /* Data only stored in the record for subsection zero.  */
    struct
    {
      /* The ELF section.  */
      Elf_Scn *scn;

      /* Entry in the section header string table.  */
      Dwelf_Strent *strent;

      /* Next member of group.  */
      struct AsmScn *next_in_group;
    } main;

    /* Pointer to the record for subsection zero.  */
    AsmScn_t *up;
  } data;

  /* Current offset in the (sub)section.  */
  GElf_Off offset;
  /* Maximum alignment of the section so far.  */
  GElf_Word max_align;

  /* Section content.  */
  struct AsmData
  {
    /* Currently used number of bytes in the block.  */
    size_t len;

    /* Number of bytes allocated.  */
    size_t maxlen;

    /* Pointer to the next block.  */
    struct AsmData *next;

    /* The actual data.  */
    char data[flexarr_size];
  } *content;

  /* Fill pattern.  */
  struct FillPattern
  {
    size_t len;
    char bytes[flexarr_size];
  } *pattern;

  /* Next subsection.  */
  AsmScn_t *subnext;

  /* List of all allocated sections.  */
  AsmScn_t *allnext;

  /* Name of the section.  */
  char name[flexarr_size];
};


/* Descriptor used for the assembling session.  */
struct AsmCtx
{
  /* File descriptor of the temporary file.  */
  int fd;

  /* True if text output is wanted.  */
  bool textp;

  /* Output file handle.  */
  union
  {
    /* ELF descriptor of the temporary file.  */
    Elf *elf;
    /* I/O stream for text output.  */
    FILE *file;
  } out;


  /* List with defined sections.  */
  AsmScn_t *section_list;
  /* Section header string table.  */
  Dwelf_Strtab *section_strtab;

  /* Table with defined symbols.  */
  asm_symbol_tab symbol_tab;
  /* Number of symbols in the table.  */
  unsigned int nsymbol_tab;
  /* Symbol string table.  */
  Dwelf_Strtab *symbol_strtab;

  /* List of section groups.  */
  struct AsmScnGrp *groups;
  /* Number of section groups.  */
  size_t ngroups;

  /* Current required alignment for common symbols.  */
  GElf_Word common_align;

  /* Lock to handle multithreaded programs.  */
  rwlock_define (,lock);

  /* Counter for temporary symbols.  */
  unsigned int tempsym_count;

  /* Name of the output file.  */
  char *fname;
  /* The name of the temporary file.  */
  char tmp_fname[flexarr_size];
};


/* Descriptor for a symbol.  */
struct AsmSym
{
  /* Reference to the section which contains the symbol.  */
  AsmScn_t *scn;

  /* Type of the symbol.  */
  int8_t type;
  /* Binding of the symbol.  */
  int8_t binding;

  /* Size of the symbol.  */
  GElf_Xword size;

  /* Offset in the section.  */
  GElf_Off offset;

  /* Symbol table index of the symbol in the symbol table.  */
  size_t symidx;

  /* Reference to name of the symbol.  */
  Dwelf_Strent *strent;
};


/* Descriptor for section group.  */
struct AsmScnGrp
{
  /* Entry in the section header string table.  */
  Dwelf_Strent *strent;

  /* The ELF section.  */
  Elf_Scn *scn;

  /* The signature.  */
  struct AsmSym *signature;

  /* First member.  */
  struct AsmScn *members;
  /* Number of members.  */
  size_t nmembers;

  /* Flags.  */
  Elf32_Word flags;

  /* Next group.  */
  struct AsmScnGrp *next;

  /* Name of the section group.  */
  char name[flexarr_size];
};


/* Descriptor for disassembler.   */
struct DisasmCtx
{
  /* Handle for the backend library with the disassembler routine.  */
  Ebl *ebl;

  /* ELF file containing all the data passed to the function.  This
     allows to look up symbols.  */
  Elf *elf;

  /* Callback function to determine symbol names.  */
  DisasmGetSymCB_t symcb;
};


/* The default fill pattern: one zero byte.  */
extern const struct FillPattern *__libasm_default_pattern
     attribute_hidden;


/* Ensure there are at least LEN bytes available in the output buffer
   for ASMSCN.  */
extern int __libasm_ensure_section_space (AsmScn_t *asmscn, size_t len)
     internal_function;

/* Free all resources associated with the assembler context.  */
extern void __libasm_finictx (AsmCtx_t *ctx) internal_function;

/* Set error code.  */
extern void __libasm_seterrno (int err) internal_function;

/* Return handle for the named section.  If it was not used before
   create it.  */
extern AsmScn_t *__asm_newscn_internal (AsmCtx_t *ctx, const char *scnname,
					GElf_Word type, GElf_Xword flags)
     attribute_hidden;


/* Internal aliases of the asm_addintXX functions.  */
extern int __asm_addint8_internal (AsmScn_t *asmscn, int8_t num)
     attribute_hidden;
extern int __asm_addint16_internal (AsmScn_t *asmscn, int16_t num)
     attribute_hidden;
extern int __asm_addint32_internal (AsmScn_t *asmscn, int32_t num)
     attribute_hidden;
extern int __asm_addint64_internal (AsmScn_t *asmscn, int64_t num)
     attribute_hidden;


/* Produce disassembly output for given memory and output it using the
   given callback functions.  */
extern int __disasm_cb_internal (DisasmCtx_t *ctx, const uint8_t **startp,
				 const uint8_t *end, GElf_Addr addr,
				 const char *fmt, DisasmOutputCB_t outcb,
				 void *outcbarp, void *symcbarg)
     attribute_hidden;


/* Test whether given symbol is an internal symbol and if yes, whether
   we should nevertheless emit it in the symbol table.  */
// XXX The second part should probably be controlled by an option which
// isn't implemented yet
// XXX Also, the format will change with the backend.
#define asm_emit_symbol_p(name) (!startswith (name, ".L"))

#endif	/* libasmP.h */
