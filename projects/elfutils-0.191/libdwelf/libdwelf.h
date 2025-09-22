/* Interfaces for libdwelf. DWARF ELF Low-level Functions.
   Copyright (C) 2014, 2015, 2016, 2018 Red Hat, Inc.
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

#ifndef _LIBDWELF_H
#define _LIBDWELF_H	1

#include "libdw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DWARF ELF Low-level Functions (dwelf).
   Functions starting with dwelf_elf will take a (libelf) Elf object as
   first argument and might set elf_errno on error.  Functions starting
   with dwelf_dwarf will take a (libdw) Dwarf object as first argument
   and might set dwarf_errno on error.  */

/* Returns the name and the CRC32 of the separate debug file from the
   .gnu_debuglink section if found in the ELF.  Return NULL if the ELF
   file didn't have a .gnu_debuglink section, had malformed data in the
   section or some other error occurred.  */
extern const char *dwelf_elf_gnu_debuglink (Elf *elf, GElf_Word *crc);

/* Returns the name and build ID from the .gnu_debugaltlink section if
   found in the ELF.  On success, pointers to the name and build ID
   are written to *NAMEP and *BUILDID_P, and the positive length of
   the build ID is returned.  Returns 0 if the ELF lacks a
   .gnu_debugaltlink section.  Returns -1 in case of malformed data or
   other errors.  */
extern ssize_t dwelf_dwarf_gnu_debugaltlink (Dwarf *dwarf,
					     const char **namep,
					     const void **build_idp);

/* Returns the build ID as found in a NT_GNU_BUILD_ID note from either
   a SHT_NOTE section or from a PT_NOTE segment if the ELF file
   doesn't contain any section headers.  On success a pointer to the
   build ID is written to *BUILDID_P, and the positive length of the
   build ID is returned.  Returns 0 if the ELF lacks a NT_GNU_BUILD_ID
   note.  Returns -1 in case of malformed data or other errors.  */
extern ssize_t dwelf_elf_gnu_build_id (Elf *elf, const void **build_idp);

/* Returns the size of the uncompressed data of a GNU compressed
   section.  The section name should start with .zdebug (but this
   isn't checked by this function).  If the section isn't compressed
   (the section data doesn't start with ZLIB) -1 is returned. If an
   error occurred -1 is returned and elf_errno is set.  */
extern ssize_t dwelf_scn_gnu_compressed_size (Elf_Scn *scn);

/* ELF/DWARF string table handling.  */
typedef struct Dwelf_Strtab Dwelf_Strtab;
typedef struct Dwelf_Strent Dwelf_Strent;

/* Create a new ELF/DWARF string table object in memory.  ELF string
   tables have a required zero length null string at offset zero.
   DWARF string tables don't require such a null entry (unless they
   are shared with an ELF string table).  If NULLSTR is true then a
   null entry is always created (even if the string table is empty
   otherwise).  */
extern Dwelf_Strtab *dwelf_strtab_init (bool nullstr);

/* Add string STR to string table ST.  Returns NULL if no memory could
   be allocated.  The given STR is owned by the called and must be
   valid till dwelf_strtab_free is called.  dwelf_strtab_finalize
   might copy the string into the final table and dwelf_strent_str
   might return it, or a reference to an identical copy/substring
   added to the string table.  */
extern Dwelf_Strent *dwelf_strtab_add (Dwelf_Strtab *st, const char *str)
  __nonnull_attribute__ (1, 2);

/* This is an optimized version of dwelf_strtab_add if the length of
   the string is already known.  LEN is the length of STR including
   zero terminator.  Calling dwelf_strtab_add (st, str) is similar to
   calling dwelf_strtab_len (st, str, strlen (str) + 1).  */
extern Dwelf_Strent *dwelf_strtab_add_len (Dwelf_Strtab *st,
					   const char *str, size_t len)
  __nonnull_attribute__ (1, 2);

/* Finalize string table ST and store size and memory location
   information in DATA d_size and d_buf.  DATA d_type will be set to
   ELF_T_BYTE, d_off will be zero, d_align will be 1 and d_version
   will be set to EV_CURRENT.  If no memory could be allocated NULL is
   returned and DATA->d_buf will be set to NULL.  Otherwise DATA will
   be returned.  */
extern Elf_Data *dwelf_strtab_finalize (Dwelf_Strtab *st,
					Elf_Data *data)
  __nonnull_attribute__ (1, 2);

/* Get offset in string table for string associated with entry.  Only
   valid after dwelf_strtab_finalize has been called.  */
extern size_t dwelf_strent_off (Dwelf_Strent *se)
  __nonnull_attribute__ (1);

/* Return the string associated with the entry.  */
extern const char *dwelf_strent_str (Dwelf_Strent *se)
  __nonnull_attribute__ (1);

/* Free resources allocated for the string table.  This invalidates
   any Dwelf_Strent references returned earlier. */
extern void dwelf_strtab_free (Dwelf_Strtab *st)
  __nonnull_attribute__ (1);

/* Creates a read-only Elf handle from the given file handle.  The
   file may be compressed and/or contain a linux kernel image header,
   in which case it is eagerly decompressed in full and the Elf handle
   is created as if created with elf_memory ().  On decompression or
   file errors NULL is returned (and elf_errno will be set).  If there
   was no error, but the file is not an ELF file, then an ELF_K_NONE
   Elf handle is returned (just like with elf_begin).  The Elf handle
   should be closed with elf_end ().  The file handle will not be
   closed.  */
extern Elf *dwelf_elf_begin (int fd);

/* Returns a human readable string for the given ELF header e_machine
   value, or NULL if the given number isn't currently known.  */
extern const char *dwelf_elf_e_machine_string (int machine);

#ifdef __cplusplus
}
#endif

#endif	/* libdwelf.h */
