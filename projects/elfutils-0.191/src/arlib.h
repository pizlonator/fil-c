/* Copyright (C) 2007-2012 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2007.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef _ARLIB_H
#define _ARLIB_H	1

#include <ar.h>
#include <argp.h>
#include <byteswap.h>
#include <endian.h>
#include <libelf.h>
#include <obstack.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


/* State of -D/-U flags.  */
extern bool arlib_deterministic_output;

/* For options common to ar and ranlib.  */
extern const struct argp_child arlib_argp_children[];


/* Maximum length of a file name that fits directly into the ar header.
   We cannot use the final byte since a / goes there.  */
#define MAX_AR_NAME_LEN (sizeof (((struct ar_hdr *) NULL)->ar_name) - 1)


/* Words matching in size to archive header.  */
#define AR_HDR_WORDS (sizeof (struct ar_hdr) / sizeof (uint32_t))


#if BYTE_ORDER == LITTLE_ENDIAN
# define le_bswap_32(val) bswap_32 (val)
#else
# define le_bswap_32(val) (val)
#endif


/* Symbol table type.  */
struct arlib_symtab
{
  /* Symbol table handling.  */
  struct obstack symsoffob;
  struct obstack symsnameob;
  size_t symsofflen;
  uint32_t *symsoff;
  size_t symsnamelen;
  char *symsname;

  /* Long filename handling.  */
  struct obstack longnamesob;
  size_t longnameslen;
  char *longnames;
};


/* Global variable with symbol table.  */
extern struct arlib_symtab symtab;


/* Initialize ARLIB_SYMTAB structure.  */
extern void arlib_init (void);

/* Finalize ARLIB_SYMTAB content.  */
extern void arlib_finalize (void);

/* Free resources for ARLIB_SYMTAB.  */
extern void arlib_fini (void);

/* Add symbols from ELF with value OFFSET to the symbol table SYMTAB.  */
extern void arlib_add_symbols (Elf *elf, const char *arfname,
			       const char *membername, off_t off);

/* Add name a file offset of a symbol.  */
extern void arlib_add_symref (const char *symname, off_t symoff);

/* Add long file name FILENAME of length FILENAMELEN to the symbol table
   SYMTAB.  Return the offset into the long file name table.  */
extern long int arlib_add_long_name (const char *filename, size_t filenamelen);

#endif	/* arlib.h */
