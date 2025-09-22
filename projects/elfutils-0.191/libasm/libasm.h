/* Interface for libasm.
   Copyright (C) 2002, 2005, 2008 Red Hat, Inc.
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

#ifndef _LIBASM_H
#define _LIBASM_H 1

#include <stdbool.h>
#include <stdint.h>
#include <gelf.h>

typedef struct ebl Ebl;


/* Opaque type for the assembler context descriptor.  */
typedef struct AsmCtx AsmCtx_t;

/* Opaque type for a section.  */
typedef struct AsmScn AsmScn_t;

/* Opaque type for a section group.  */
typedef struct AsmScnGrp AsmScnGrp_t;

/* Opaque type for a symbol.  */
typedef struct AsmSym AsmSym_t;


/* Opaque type for the disassembler context descriptor.  */
typedef struct DisasmCtx DisasmCtx_t;

/* Type used for callback functions to retrieve symbol name.  The
   symbol reference is in the section designated by the second parameter
   at an offset described by the first parameter.  The value is the
   third parameter.  */
typedef int (*DisasmGetSymCB_t) (GElf_Addr, Elf32_Word, GElf_Addr, char **,
				 size_t *, void *);

/* Output function callback.  */
typedef int (*DisasmOutputCB_t) (char *, size_t, void *);


#ifdef __cplusplus
extern "C" {
#endif

/* Create output file and return descriptor for assembler context.  If
   TEXTP is true the output is an assembler format text file.
   Otherwise an object file is created.  The MACHINE parameter
   corresponds to an EM_ constant from <elf.h>, KLASS specifies the
   class (32- or 64-bit), and DATA specifies the byte order (little or
   big endian).  */
extern AsmCtx_t *asm_begin (const char *fname, Ebl *ebl, bool textp);

/* Abort the operation on the assembler context and free all resources.  */
extern int asm_abort (AsmCtx_t *ctx);

/* Finalize output file and free all resources.  */
extern int asm_end (AsmCtx_t *ctx);


/* Return handle for the named section.  If it was not used before
   create it.  */
extern AsmScn_t *asm_newscn (AsmCtx_t *ctx, const char *scnname,
			     GElf_Word type, GElf_Xword flags);


/* Similar to 'asm_newscn', but make it part of section group GRP.  */
extern AsmScn_t *asm_newscn_ingrp (AsmCtx_t *ctx, const char *scnname,
				   GElf_Word type, GElf_Xword flags,
				   AsmScnGrp_t *grp);

/* Create new subsection NR in the given section.  */
extern AsmScn_t *asm_newsubscn (AsmScn_t *asmscn, unsigned int nr);


/* Return handle for new section group.  The signature symbol can be
   set later.  */
extern AsmScnGrp_t *asm_newscngrp (AsmCtx_t *ctx, const char *grpname,
				   AsmSym_t *signature, Elf32_Word flags);

/* Set or overwrite signature symbol for group.  */
extern int asm_scngrp_newsignature (AsmScnGrp_t *grp, AsmSym_t *signature);


/* Add zero terminated string STR of size LEN to (sub)section ASMSCN.  */
extern int asm_addstrz (AsmScn_t *asmscn, const char *str, size_t len);

/* Add 8-bit signed integer NUM to (sub)section ASMSCN.  */
extern int asm_addint8 (AsmScn_t *asmscn, int8_t num);

/* Add 8-bit unsigned integer NUM to (sub)section ASMSCN.  */
extern int asm_adduint8 (AsmScn_t *asmscn, uint8_t num);

/* Add 16-bit signed integer NUM to (sub)section ASMSCN.  */
extern int asm_addint16 (AsmScn_t *asmscn, int16_t num);

/* Add 16-bit unsigned integer NUM to (sub)section ASMSCN.  */
extern int asm_adduint16 (AsmScn_t *asmscn, uint16_t num);

/* Add 32-bit signed integer NUM to (sub)section ASMSCN.  */
extern int asm_addint32 (AsmScn_t *asmscn, int32_t num);

/* Add 32-bit unsigned integer NUM to (sub)section ASMSCN.  */
extern int asm_adduint32 (AsmScn_t *asmscn, uint32_t num);

/* Add 64-bit signed integer NUM to (sub)section ASMSCN.  */
extern int asm_addint64 (AsmScn_t *asmscn, int64_t num);

/* Add 64-bit unsigned integer NUM to (sub)section ASMSCN.  */
extern int asm_adduint64 (AsmScn_t *asmscn, uint64_t num);


/* Add signed little endian base 128 integer NUM to (sub)section ASMSCN.  */
extern int asm_addsleb128 (AsmScn_t *asmscn, int32_t num);

/* Add unsigned little endian base 128 integer NUM to (sub)section ASMSCN.  */
extern int asm_adduleb128 (AsmScn_t *asmscn, uint32_t num);


/* Define new symbol NAME for current position in given section ASMSCN.  */
extern AsmSym_t *asm_newsym (AsmScn_t *asmscn, const char *name,
			     GElf_Xword size, int type, int binding);


/* Define new common symbol NAME with given SIZE and alignment.  */
extern AsmSym_t *asm_newcomsym (AsmCtx_t *ctx, const char *name,
				GElf_Xword size, GElf_Addr align);

/* Define new common symbol NAME with given SIZE, VALUE, TYPE, and BINDING.  */
extern AsmSym_t *asm_newabssym (AsmCtx_t *ctx, const char *name,
				GElf_Xword size, GElf_Addr value,
				int type, int binding);


/* Align (sub)section offset according to VALUE.  */
extern int asm_align (AsmScn_t *asmscn, GElf_Word value);

/* Set the byte pattern used to fill gaps created by alignment.  */
extern int asm_fill (AsmScn_t *asmscn, void *bytes, size_t len);


/* Return ELF descriptor created for the output file of the given context.  */
extern Elf *asm_getelf (AsmCtx_t *ctx);


/* Return error code of last failing function call.  This value is kept
   separately for each thread.  */
extern int asm_errno (void);

/* Return error string for ERROR.  If ERROR is zero, return error string
   for most recent error or NULL is none occurred.  If ERROR is -1 the
   behaviour is similar to the last case except that not NULL but a legal
   string is returned.  */
extern const char *asm_errmsg (int __error);


/* Create context descriptor for disassembler.  */
extern DisasmCtx_t *disasm_begin (Ebl *ebl, Elf *elf, DisasmGetSymCB_t symcb);

/* Release descriptor for disassembler.  */
extern int disasm_end (DisasmCtx_t *ctx);

/* Produce of disassembly output for given memory, store text in
   provided buffer.  */
extern int disasm_str (DisasmCtx_t *ctx, const uint8_t **startp,
		       const uint8_t *end, GElf_Addr addr, const char *fmt,
		       char **bufp, size_t len, void *symcbarg);

/* Produce disassembly output for given memory and output it using the
   given callback functions.  */
extern int disasm_cb (DisasmCtx_t *ctx, const uint8_t **startp,
		      const uint8_t *end, GElf_Addr addr, const char *fmt,
		      DisasmOutputCB_t outcb, void *outcbarg, void *symcbarg);

#ifdef __cplusplus
}
#endif

#endif	/* libasm.h */
