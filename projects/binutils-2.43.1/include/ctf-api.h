/* Public API to libctf.
   Copyright (C) 2019-2024 Free Software Foundation, Inc.

   This file is part of libctf.

   libctf is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not see
   <http://www.gnu.org/licenses/>.  */

/* This header file defines the interfaces available from the CTF debugger
   library, libctf.  This API can be used by a debugger to operate on data in
   the Compact ANSI-C Type Format (CTF).  */

#ifndef	_CTF_API_H
#define	_CTF_API_H

#include <sys/types.h>
#include <inttypes.h>
#include <ctf.h>
#include <zlib.h>

#ifdef	__cplusplus
extern "C"
{
#endif

/* Clients can open one or more CTF containers and obtain a pointer to an
   opaque ctf_dict_t.  Types are identified by an opaque ctf_id_t token.
   They can also open or create read-only archives of CTF containers in a
   ctf_archive_t.

   These opaque definitions allow libctf to evolve without breaking clients.  */

typedef struct ctf_dict ctf_dict_t;
typedef struct ctf_archive_internal ctf_archive_t;
typedef unsigned long ctf_id_t;

/* This opaque definition allows libctf to accept BFD data structures without
   importing all the BFD noise into users' namespaces.  */

struct bfd;

/* If the debugger needs to provide the CTF library with a set of raw buffers
   for use as the CTF data, symbol table, and string table, it can do so by
   filling in ctf_sect_t structures and passing them to ctf_bufopen.

   The contents of this structure must always be in native endianness.  At read
   time, the symbol table endianness is derived from the BFD target (if BFD is
   in use): if a BFD target is not in use, please call ctf_symsect_endianness or
   ctf_arc_symsect_endianness.  */

typedef struct ctf_sect
{
  const char *cts_name;		  /* Section name (if any).  */
  const void *cts_data;		  /* Pointer to section data.  */
  size_t cts_size;		  /* Size of data in bytes.  */
  size_t cts_entsize;		  /* Size of each section entry (symtab only).  */
} ctf_sect_t;

/* A minimal symbol extracted from a linker's internal symbol table
   representation.  The symbol name can be given either via st_name or via a
   strtab offset in st_nameidx, which corresponds to one of the string offsets
   communicated via the ctf_link_add_strtab callback.   */

typedef struct ctf_link_sym
{
  /* The st_name and st_nameidx will not be accessed outside the call to
     ctf_link_shuffle_syms.  If you set st_nameidx to offset zero, make sure
     to set st_nameidx_set as well.  */

  const char *st_name;
  size_t st_nameidx;
  int st_nameidx_set;
  uint32_t st_symidx;
  uint32_t st_shndx;
  uint32_t st_type;
  uint32_t st_value;
} ctf_link_sym_t;

/* Flags applying to this specific link.  */

/* Share all types that are not in conflict.  The default.  */
#define CTF_LINK_SHARE_UNCONFLICTED 0x0

/* Share only types that are used by multiple inputs.  */
#define CTF_LINK_SHARE_DUPLICATED 0x1

/* Do a nondeduplicating link, or otherwise deduplicate "less hard", trading off
   CTF output size for link time.  */
#define CTF_LINK_NONDEDUP 0x2

/* Create empty outputs for all registered CU mappings even if no types are
   emitted into them.  */
#define CTF_LINK_EMPTY_CU_MAPPINGS 0x4

/* Omit the content of the variables section.  */
#define CTF_LINK_OMIT_VARIABLES_SECTION 0x8

/* If *unset*, filter out entries corresponding to linker-reported symbols
   from the variable section, and filter out all entries with no linker-reported
   symbols from the data object and function info sections: if set, do no
   filtering and leave all entries in place.  (This is a negative-sense flag
   because it is rare to want symbols the linker has not reported as present to
   stick around in the symtypetab sections nonetheless: relocatable links are
   the only likely case.)  */
#define CTF_LINK_NO_FILTER_REPORTED_SYMS 0x10

/* Symbolic names for CTF sections.  */

typedef enum ctf_sect_names
  {
   CTF_SECT_HEADER,
   CTF_SECT_LABEL,
   CTF_SECT_OBJT,
   CTF_SECT_OBJTIDX = CTF_SECT_OBJT,
   CTF_SECT_FUNC,
   CTF_SECT_FUNCIDX = CTF_SECT_FUNC,
   CTF_SECT_VAR,
   CTF_SECT_TYPE,
   CTF_SECT_STR
  } ctf_sect_names_t;

/* Encoding information for integers, floating-point values, and certain other
   intrinsics can be obtained by calling ctf_type_encoding, below.  The flags
   field will contain values appropriate for the type defined in <ctf.h>.  */

typedef struct ctf_encoding
{
  uint32_t cte_format;		 /* Data format (CTF_INT_* or CTF_FP_* flags).  */
  uint32_t cte_offset;		 /* Offset of value in bits.  */
  uint32_t cte_bits;		 /* Size of storage in bits.  */
} ctf_encoding_t;

typedef struct ctf_membinfo
{
  ctf_id_t ctm_type;		/* Type of struct or union member.  */
  unsigned long ctm_offset;	/* Offset of member in bits.  */
} ctf_membinfo_t;

typedef struct ctf_arinfo
{
  ctf_id_t ctr_contents;	/* Type of array contents.  */
  ctf_id_t ctr_index;		/* Type of array index.  */
  uint32_t ctr_nelems;		/* Number of elements.  */
} ctf_arinfo_t;

typedef struct ctf_funcinfo
{
  ctf_id_t ctc_return;		/* Function return type.  */
  uint32_t ctc_argc;		/* Number of typed arguments to function.  */
  uint32_t ctc_flags;		/* Function attributes (see below).  */
} ctf_funcinfo_t;

typedef struct ctf_lblinfo
{
  ctf_id_t ctb_type;		/* Last type associated with the label.  */
} ctf_lblinfo_t;

typedef struct ctf_snapshot_id
{
  unsigned long dtd_id;		/* Highest DTD ID at time of snapshot.  */
  unsigned long snapshot_id;	/* Snapshot id at time of snapshot.  */
} ctf_snapshot_id_t;

#define	CTF_FUNC_VARARG	0x1	/* Function arguments end with varargs.  */

/* Functions that return a ctf_id_t use the following value to indicate failure.
   ctf_errno can be used to obtain an error code.  Functions that return
   a straight integral -1 also use ctf_errno.  */
#define	CTF_ERR	((ctf_id_t) -1L)

/* This macro holds information about all the available ctf errors.
   It is used to form both an enum holding all the error constants,
   and also the error strings themselves.  To use, define _CTF_FIRST
   and _CTF_ITEM to expand as you like, then mention the macro name.
   See the enum after this for an example.  */
#define _CTF_ERRORS \
  _CTF_FIRST (ECTF_FMT, "File is not in CTF or ELF format.")	\
  _CTF_ITEM (ECTF_BFDERR, "BFD error.")				\
  _CTF_ITEM (ECTF_CTFVERS, "CTF dict version is too new for libctf.") \
  _CTF_ITEM (ECTF_BFD_AMBIGUOUS, "Ambiguous BFD target.")	\
  _CTF_ITEM (ECTF_SYMTAB, "Symbol table uses invalid entry size.") \
  _CTF_ITEM (ECTF_SYMBAD, "Symbol table data buffer is not valid.") \
  _CTF_ITEM (ECTF_STRBAD, "String table data buffer is not valid.") \
  _CTF_ITEM (ECTF_CORRUPT, "File data structure corruption detected.") \
  _CTF_ITEM (ECTF_NOCTFDATA, "File does not contain CTF data.") \
  _CTF_ITEM (ECTF_NOCTFBUF, "Buffer does not contain CTF data.") \
  _CTF_ITEM (ECTF_NOSYMTAB, "Symbol table information is not available.") \
  _CTF_ITEM (ECTF_NOPARENT, "The parent CTF dictionary is needed but unavailable.") \
  _CTF_ITEM (ECTF_DMODEL, "Data model mismatch.") \
  _CTF_ITEM (ECTF_LINKADDEDLATE, "File added to link too late.") \
  _CTF_ITEM (ECTF_ZALLOC, "Failed to allocate (de)compression buffer.") \
  _CTF_ITEM (ECTF_DECOMPRESS, "Failed to decompress CTF data.") \
  _CTF_ITEM (ECTF_STRTAB, "External string table is not available.") \
  _CTF_ITEM (ECTF_BADNAME, "String name offset is corrupt.") \
  _CTF_ITEM (ECTF_BADID, "Invalid type identifier.") \
  _CTF_ITEM (ECTF_NOTSOU, "Type is not a struct or union.") \
  _CTF_ITEM (ECTF_NOTENUM, "Type is not an enum.") \
  _CTF_ITEM (ECTF_NOTSUE, "Type is not a struct, union, or enum.") \
  _CTF_ITEM (ECTF_NOTINTFP, "Type is not an integer, float, or enum.") \
  _CTF_ITEM (ECTF_NOTARRAY, "Type is not an array.") \
  _CTF_ITEM (ECTF_NOTREF, "Type does not reference another type.") \
  _CTF_ITEM (ECTF_NAMELEN, "Buffer is too small to hold type name.") \
  _CTF_ITEM (ECTF_NOTYPE, "No type found corresponding to name.") \
  _CTF_ITEM (ECTF_SYNTAX, "Syntax error in type name.") \
  _CTF_ITEM (ECTF_NOTFUNC, "Symbol table entry or type is not a function.") \
  _CTF_ITEM (ECTF_NOFUNCDAT, "No function information available for function.") \
  _CTF_ITEM (ECTF_NOTDATA, "Symbol table entry does not refer to a data object.") \
  _CTF_ITEM (ECTF_NOTYPEDAT, "No type information available for symbol.") \
  _CTF_ITEM (ECTF_NOLABEL, "No label found corresponding to name.") \
  _CTF_ITEM (ECTF_NOLABELDATA, "File does not contain any labels.") \
  _CTF_ITEM (ECTF_NOTSUP, "Feature not supported.") \
  _CTF_ITEM (ECTF_NOENUMNAM, "Enumerator name not found.") \
  _CTF_ITEM (ECTF_NOMEMBNAM, "Member name not found.") \
  _CTF_ITEM (ECTF_RDONLY, "CTF container is read-only.") \
  _CTF_ITEM (ECTF_DTFULL, "CTF type is full (no more members allowed).") \
  _CTF_ITEM (ECTF_FULL, "CTF container is full.") \
  _CTF_ITEM (ECTF_DUPLICATE, "Duplicate member, enumerator, or variable name.") \
  _CTF_ITEM (ECTF_CONFLICT, "Conflicting type is already defined.") \
  _CTF_ITEM (ECTF_OVERROLLBACK, "Attempt to roll back past a ctf_update.") \
  _CTF_ITEM (ECTF_COMPRESS, "Failed to compress CTF data.") \
  _CTF_ITEM (ECTF_ARCREATE, "Error creating CTF archive.") \
  _CTF_ITEM (ECTF_ARNNAME, "Name not found in CTF archive.") \
  _CTF_ITEM (ECTF_SLICEOVERFLOW, "Overflow of type bitness or offset in slice.") \
  _CTF_ITEM (ECTF_DUMPSECTUNKNOWN, "Unknown section number in dump.") \
  _CTF_ITEM (ECTF_DUMPSECTCHANGED, "Section changed in middle of dump.") \
  _CTF_ITEM (ECTF_NOTYET, "Feature not yet implemented.") \
  _CTF_ITEM (ECTF_INTERNAL, "Internal error: assertion failure.") \
  _CTF_ITEM (ECTF_NONREPRESENTABLE, "Type not representable in CTF.") \
  _CTF_ITEM (ECTF_NEXT_END, "End of iteration.") \
  _CTF_ITEM (ECTF_NEXT_WRONGFUN, "Wrong iteration function called.") \
  _CTF_ITEM (ECTF_NEXT_WRONGFP, "Iteration entity changed in mid-iterate.") \
  _CTF_ITEM (ECTF_FLAGS, "CTF header contains flags unknown to libctf.") \
  _CTF_ITEM (ECTF_NEEDSBFD, "This feature needs a libctf with BFD support.") \
  _CTF_ITEM (ECTF_INCOMPLETE, "Type is not a complete type.") \
  _CTF_ITEM (ECTF_NONAME, "Type name must not be empty.") \
  _CTF_ITEM (ECTF_BADFLAG, "Invalid CTF dict flag specified.")

#define	ECTF_BASE	1000	/* Base value for libctf errnos.  */

enum
  {
#define _CTF_FIRST(NAME, STR) NAME = ECTF_BASE
#define _CTF_ITEM(NAME, STR) , NAME
_CTF_ERRORS
#undef _CTF_ITEM
#undef _CTF_FIRST
  };

#define ECTF_NERR (ECTF_BADFLAG - ECTF_BASE + 1) /* Count of CTF errors.  */

/* The CTF data model is inferred to be the caller's data model or the data
   model of the given object, unless ctf_setmodel is explicitly called.  */
#define	CTF_MODEL_ILP32 1	/* Object data model is ILP32.  */
#define	CTF_MODEL_LP64  2	/* Object data model is LP64.  */
#ifdef _LP64
# define CTF_MODEL_NATIVE CTF_MODEL_LP64
#else
# define CTF_MODEL_NATIVE CTF_MODEL_ILP32
#endif

/* Dynamic CTF containers can be created using ctf_create.  The ctf_add_*
   routines can be used to add new definitions to the dynamic container.  New
   types are labeled as root or non-root to determine whether they are visible
   at the top-level program scope when subsequently doing a lookup.
   (Identifiers contained within non-root types, like enumeration constants, are
   also not visible.)  */

#define	CTF_ADD_NONROOT	0	/* Type only visible in nested scope.  */
#define	CTF_ADD_ROOT	1	/* Type visible at top-level scope.  */

/* Flags for ctf_member_next.  */

#define CTF_MN_RECURSE 0x1	/* Recurse into unnamed members.  */

/* Flags for ctf_dict_set_flag.  */

/* If set, duplicate enumerators in a single dict fail with ECTF_DUPLICATE.  */

#define CTF_STRICT_NO_DUP_ENUMERATORS	0x1

/* These typedefs are used to define the signature for callback functions that
   can be used with the iteration and visit functions below.  There is also a
   family of iteration functions that do not require callbacks.  */

typedef int ctf_visit_f (const char *name, ctf_id_t type, unsigned long offset,
			 int depth, void *arg);
typedef int ctf_member_f (const char *name, ctf_id_t membtype,
			  unsigned long offset, void *arg);
typedef int ctf_enum_f (const char *name, int val, void *arg);
typedef int ctf_variable_f (const char *name, ctf_id_t type, void *arg);
typedef int ctf_type_f (ctf_id_t type, void *arg);
typedef int ctf_type_all_f (ctf_id_t type, int flag, void *arg);
typedef int ctf_label_f (const char *name, const ctf_lblinfo_t *info,
			 void *arg);
typedef int ctf_archive_member_f (ctf_dict_t *fp, const char *name, void *arg);
typedef int ctf_archive_raw_member_f (const char *name, const void *content,
				      size_t len, void *arg);
typedef char *ctf_dump_decorate_f (ctf_sect_names_t sect,
				   char *line, void *arg);

typedef struct ctf_dump_state ctf_dump_state_t;

/* Iteration state for the _next functions, and allocators/copiers/freers for
   it.  (None of these are needed for the simple case of iterating to the end:
   the _next functions allocate and free the iterators for you.)

   The _next iterators all work in similar ways: they take things to query (a
   dict, a name, a type ID, something like that), then a ctf_next_t iterator
   arg which must be the address of a variable whose value is NULL on first
   call, and will be set to NULL again once iteration has completed.

   They return something important about the thing being iterated over (often a
   type ID or a name); on end of iteration they instead return return CTF_ERR,
   -1, or NULL and set the error ECTF_NEXT_END on the dict.  They can often
   provide more information too: this is done via pointer parameters (e.g. the
   membname and membtype in ctf_member_next()).  These parameters are always
   optional and can be set to NULL if not needed.

   Errors other than end-of-iteration will return CTF_ERR/-1/NULL and set the
   error to something other than ECTF_NEXT_END, and *not* destroy the iterator:
   you should either recover somehow and continue iterating, or call
   ctf_next_destroy() on it.  (You can call ctf_next_destroy() on a NULL
   iterator, so it's safe to just unconditionally do it after iteration has
   completed.)  */

typedef struct ctf_next ctf_next_t;
extern ctf_next_t *ctf_next_create (void);
extern void ctf_next_destroy (ctf_next_t *);
extern ctf_next_t *ctf_next_copy (ctf_next_t *);

/* Opening.  These mostly return an abstraction over both CTF files and CTF
   archives: so they can be used to open both.  CTF files will appear to be an
   archive with one member named '.ctf'.

   All these functions except for ctf_close use BFD and can open anything BFD
   can open, hunting down the .ctf section for you, so are not available in the
   libctf-nobfd flavour of the library.  If you want to provide the CTF section
   yourself, you can do that with ctf_bfdopen_ctfsect.  */

extern ctf_archive_t *ctf_bfdopen (struct bfd *, int *);
extern ctf_archive_t *ctf_bfdopen_ctfsect (struct bfd *, const ctf_sect_t *,
					   int *);
extern ctf_archive_t *ctf_fdopen (int fd, const char *filename,
				  const char *target, int *errp);
extern ctf_archive_t *ctf_open (const char *filename,
				const char *target, int *errp);
extern void ctf_close (ctf_archive_t *);

/* Set or unset dict-wide boolean flags, and get the value of these flags.  */

extern int ctf_dict_set_flag (ctf_dict_t *, uint64_t flag, int set);
extern int ctf_dict_get_flag (ctf_dict_t *, uint64_t flag);

/* Return the data, symbol, or string sections used by a given CTF dict.  */
extern ctf_sect_t ctf_getdatasect (const ctf_dict_t *);
extern ctf_sect_t ctf_getsymsect (const ctf_dict_t *);
extern ctf_sect_t ctf_getstrsect (const ctf_dict_t *);

/* Set the endianness of the symbol section, which may be different from
   the endianness of the CTF dict. Done for you by ctf_open and ctf_fdopen,
   but direct calls to ctf_bufopen etc with symbol sections provided must
   do so explicitly.  */

extern void ctf_symsect_endianness (ctf_dict_t *, int little_endian);
extern void ctf_arc_symsect_endianness (ctf_archive_t *, int little_endian);

/* Open CTF archives from files or raw section data, and close them again.
   Closing may munmap() the data making up the archive, so should not be
   done until all dicts are finished with and closed themselves.

   Almost all functions that open archives will also open raw CTF dicts, which
   are treated as if they were archives with only one member.

   Some of these functions take optional raw symtab and strtab section content
   in the form of ctf_sect_t structures.  For CTF in ELF files, the more
   convenient opening functions above extract these .dynsym and its associated
   string table (usually .dynsym) whenever the CTF_F_DYNSTR flag is set in the
   CTF preamble (which it almost always will be for linked objects, but not for
   .o files).  If you use ctf_arc_bufopen and do not specify symbol/string
   tables, the ctf_*_lookuup_symbol functions will fail with ECTF_NOSYMTAB.

   Like many other convenient opening functions, ctf_arc_open needs BFD and is
   not available in libctf-nobfd.  */

extern ctf_archive_t *ctf_arc_open (const char *, int *);
extern ctf_archive_t *ctf_arc_bufopen (const ctf_sect_t *ctfsect,
				       const ctf_sect_t *symsect,
				       const ctf_sect_t *strsect,
				       int *);
extern void ctf_arc_close (ctf_archive_t *);

/* Get the archive a given dictionary came from (if any).  */

extern ctf_archive_t *ctf_get_arc (const ctf_dict_t *);

/* Return the number of members in an archive.  */

extern size_t ctf_archive_count (const ctf_archive_t *);

/* Open a dictionary with a given name, given a CTF archive and
   optionally symbol and string table sections to accompany it (if the
   archive was oriiginally opened from an ELF file via ctf_open*, or
   if string or symbol tables were explicitly passed when the archive
   was opened, this can be used to override that choice).  The dict
   should be closed with ctf_dict_close() when done.

   (The low-level functions ctf_simple_open and ctf_bufopen return
   ctf_dict_t's directly, and cannot be used on CTF archives: use these
   functions instead.)  */

extern ctf_dict_t *ctf_dict_open (const ctf_archive_t *,
				  const char *, int *);
extern ctf_dict_t *ctf_dict_open_sections (const ctf_archive_t *,
					   const ctf_sect_t *symsect,
					   const ctf_sect_t *strsect,
					   const char *, int *);

/* Look up symbols' types in archives by index or name, returning the dict
   and optionally type ID in which the type is found.  Lookup results are
   cached so future lookups are faster.  Needs symbol tables and (for name
   lookups) string tables to be known for this CTF archive.  */

extern ctf_dict_t *ctf_arc_lookup_symbol (ctf_archive_t *,
					  unsigned long symidx,
					  ctf_id_t *, int *errp);
extern ctf_dict_t *ctf_arc_lookup_symbol_name (ctf_archive_t *,
					       const char *name,
					       ctf_id_t *, int *errp);
extern void ctf_arc_flush_caches (ctf_archive_t *);

/* The next functions return or close real CTF files, or write out CTF
   archives, not archives or ELF files containing CTF content.  As with
   ctf_dict_open_sections, they can be passed symbol and string table
   sections.  */

extern ctf_dict_t *ctf_simple_open (const char *ctfsect, size_t ctfsect_size,
				    const char *symsect, size_t symsect_size,
				    size_t symsect_entsize,
				    const char *strsect, size_t strsect_size,
				    int *errp);
extern ctf_dict_t *ctf_bufopen (const ctf_sect_t *ctfsect,
				const ctf_sect_t *symsect,
				const ctf_sect_t *strsect, int *);
extern void ctf_ref (ctf_dict_t *);
extern void ctf_dict_close (ctf_dict_t *);

/* CTF dicts may be in a parent/child relationship, where the child dicts
   contain the name of their originating compilation unit and the name of
   their parent.  Dicts opened from CTF archives have this relationship set
   up already, but if opening via raw low-level calls, you need to figure
   out which dict is the parent and set it on the child via ctf_import(). */

extern const char *ctf_cuname (ctf_dict_t *);
extern ctf_dict_t *ctf_parent_dict (ctf_dict_t *);
extern const char *ctf_parent_name (ctf_dict_t *);
extern int ctf_type_isparent (ctf_dict_t *, ctf_id_t);
extern int ctf_type_ischild (ctf_dict_t *, ctf_id_t);
extern int ctf_import (ctf_dict_t *, ctf_dict_t *);

/* Set these names (used when creating dicts).  */

extern int ctf_cuname_set (ctf_dict_t *, const char *);
extern int ctf_parent_name_set (ctf_dict_t *, const char *);

/* Set and get the CTF data model (see above).  */

extern int ctf_setmodel (ctf_dict_t *, int);
extern int ctf_getmodel (ctf_dict_t *);

/* CTF dicts can carry a single (in-memory-only) non-persistent pointer to
   arbitrary data.  No meaning is attached to this data and the dict does
   not own it: nothing is done to it when the dict is closed.  */

extern void ctf_setspecific (ctf_dict_t *, void *);
extern void *ctf_getspecific (ctf_dict_t *);

/* Error handling.  ctf dicts carry a system errno value or one of the
   CTF_ERRORS above, which are returned via ctf_errno.  The return value of
   ctf_errno is only meaningful when the immediately preceding CTF function
   call returns an error code.

   There are four possible sorts of error return:

    - From opening functions, a return value of NULL and the error returned
      via an errp instead of via ctf_errno; all other functions return return
      errors via ctf_errno.

    - Functions returning a ctf_id_t are in error if the return value == CTF_ERR
    - Functions returning an int are in error if their return value < 0
    - Functions returning a pointer are in error if their return value ==
      NULL.  */

extern int ctf_errno (ctf_dict_t *);
extern const char *ctf_errmsg (int);

/* Return the version of CTF dicts written by writeout functions.  The
   argument must currently be zero.  All dicts with versions below the value
   returned by this function can be read by the library.  CTF dicts written
   by other non-GNU CTF libraries (e.g. that in FreeBSD) are not compatible
   and cannot be read by this library.  */

extern int ctf_version (int);

/* Given a symbol table index corresponding to a function symbol, return info on
   the type of a given function's arguments or return value.  Vararg functions
   have a final arg with CTF_FUNC_VARARG on in ctc_flags.  */

extern int ctf_func_info (ctf_dict_t *, unsigned long, ctf_funcinfo_t *);
extern int ctf_func_args (ctf_dict_t *, unsigned long, uint32_t, ctf_id_t *);

/* As above, but for CTF_K_FUNCTION types in CTF dicts.  */

extern int ctf_func_type_info (ctf_dict_t *, ctf_id_t, ctf_funcinfo_t *);
extern int ctf_func_type_args (ctf_dict_t *, ctf_id_t, uint32_t, ctf_id_t *);

/* Look up function or data symbols by name and return their CTF type ID,
  if any.  (For both function symbols and data symbols that are function
  pointers, the types are of kind CTF_K_FUNCTION.)  */

extern ctf_id_t ctf_lookup_by_symbol (ctf_dict_t *, unsigned long);
extern ctf_id_t ctf_lookup_by_symbol_name (ctf_dict_t *, const char *);

/* Traverse all (function or data) symbols in a dict, one by one, and return the
   type of each and (if NAME is non-NULL) optionally its name.  */

extern ctf_id_t ctf_symbol_next (ctf_dict_t *, ctf_next_t **,
				 const char **name, int functions);

/* Look up a type by name: some simple C type parsing is done, but this is by no
   means comprehensive.  Structures, unions and enums need "struct ", "union "
   or "enum " on the front, as usual in C.  */

extern ctf_id_t ctf_lookup_by_name (ctf_dict_t *, const char *);

/* Look up a variable, which is a name -> type mapping with no specific
   relationship to a symbol table.  Before linking, everything with types in the
   symbol table will be in the variable table as well; after linking, only those
   typed functions and data objects that are not asssigned to symbols by the
   linker are left in the variable table here.  */

extern ctf_id_t ctf_lookup_variable (ctf_dict_t *, const char *);

/* Look up a single enumerator by enumeration constant name.  Returns the ID of
   the enum it is contained within and optionally its value.  Error out with
   ECTF_DUPLICATE if multiple exist (which can happen in some older dicts).  See
   ctf_lookup_enumerator_next in that case.  Enumeration constants in non-root
   types are not returned, but constants in parents are, if not overridden by
   an enum in the child.  */

extern ctf_id_t ctf_lookup_enumerator (ctf_dict_t *, const char *,
				       int64_t *enum_value);

/* Type lookup functions.  */

/* Strip qualifiers and typedefs off a type, returning the base type.

   Stripping also stops when we hit slices (see ctf_add_slice below), so it is
   possible (given a chain looking like const -> slice -> typedef -> int) to
   still have a typedef after you're done with this, but in that case it is a
   typedef of a type with a *different width* (because this slice has not been
   applied to it).

   Most of the time you don't need to call this: the type-querying functions
   will do it for you (as noted below).  */

extern ctf_id_t ctf_type_resolve (ctf_dict_t *, ctf_id_t);

/* Get the name of a type, including any const/volatile/restrict qualifiers
   (cvr-quals), and return it as a new dynamically-allocated string.
   (The 'a' stands for 'a'llocated.) */

extern char *ctf_type_aname (ctf_dict_t *, ctf_id_t);

/* As above, but with no cvr-quals.  */

extern char *ctf_type_aname_raw (ctf_dict_t *, ctf_id_t);

/* A raw name that is owned by the ctf_dict_t and will live as long as it
   does.  Do not change the value this function returns!  */

extern const char *ctf_type_name_raw (ctf_dict_t *, ctf_id_t);

/* Like ctf_type_aname, but print the string into the passed buffer, truncating
   if necessary and setting ECTF_NAMELEN on the errno: return the actual number
   of bytes needed (not including the trailing \0).  Consider using
   ctf_type_aname instead.  */

extern ssize_t ctf_type_lname (ctf_dict_t *, ctf_id_t, char *, size_t);

/* Like ctf_type_lname, but return the string, or NULL if truncated.
   Consider using ctf_type_aname instead.  */

extern char *ctf_type_name (ctf_dict_t *, ctf_id_t, char *, size_t);

/* Return the size or alignment of a type.  Types with no meaningful size, like
   function types, return 0 as their size; incomplete types set ECTF_INCOMPLETE.
   The type is resolved for you, so cvr-quals and typedefs can be passsed in.  */

extern ssize_t ctf_type_size (ctf_dict_t *, ctf_id_t);
extern ssize_t ctf_type_align (ctf_dict_t *, ctf_id_t);

/* Return the kind of a type (CTF_K_* constant).  Slices are considered to be
   the kind they are a slice of.  Forwards to incomplete structs, etc, return
   CTF_K_FORWARD (but deduplication resolves most forwards to their concrete
   types).  */

extern int ctf_type_kind (ctf_dict_t *, ctf_id_t);

/* Return the kind of a type (CTF_K_* constant).  Slices are considered to be
   the kind they are a slice of; forwards are considered to be the kind they are
   a forward of.  */

extern int ctf_type_kind_forwarded (ctf_dict_t *, ctf_id_t);

/* Return the type a pointer, typedef, cvr-qual, or slice refers to, or return
   an ECTF_NOTREF error otherwise.  ctf_type_kind pretends that slices are
   actually the type they are a slice of: this is usually want you want, but if
   you want to find out if a type was actually a slice of some (usually-wider)
   base type, you can call ctf_type_reference on it: a non-error return means
   it was a slice.  */

extern ctf_id_t ctf_type_reference (ctf_dict_t *, ctf_id_t);

/* Return the encoding of a given type.  No attempt is made to resolve the
   type first, so passing in typedefs etc will yield an error.  */

extern int ctf_type_encoding (ctf_dict_t *, ctf_id_t, ctf_encoding_t *);

/* Given a type, return some other type that is a pointer to this type (if any
   exists), or return ECTF_NOTYPE otherwise.  If non exists, try resolving away
   typedefs and cvr-quals and check again (so if you call this on foo_t, you
   might get back foo *).  No attempt is made to hunt for pointers to qualified
   versions of the type passed in.  */

extern ctf_id_t ctf_type_pointer (ctf_dict_t *, ctf_id_t);

/* Return 1 if two types are assignment-compatible.  */

extern int ctf_type_compat (ctf_dict_t *, ctf_id_t, ctf_dict_t *, ctf_id_t);

/* Recursively visit the members of any type, calling the ctf_visit_f for each.  */

extern int ctf_type_visit (ctf_dict_t *, ctf_id_t, ctf_visit_f *, void *);

/* Comparison function that defines an ordering over types.  If the types are in
   different dicts, the ordering may vary between different openings of the same
   dicts.  */

extern int ctf_type_cmp (ctf_dict_t *, ctf_id_t, ctf_dict_t *, ctf_id_t);

/* Get the name of an enumerator given its value, or vice versa.  If many
   enumerators have the same value, the first with that value is returned.  */

extern const char *ctf_enum_name (ctf_dict_t *, ctf_id_t, int);
extern int ctf_enum_value (ctf_dict_t *, ctf_id_t, const char *, int *);

/* Get the size and member type of an array.  */

extern int ctf_array_info (ctf_dict_t *, ctf_id_t, ctf_arinfo_t *);

/* Get info on specific named members of structs or unions, and count the number
   of members in a struct, union, or enum.  */

extern int ctf_member_info (ctf_dict_t *, ctf_id_t, const char *,
			    ctf_membinfo_t *);
extern int ctf_member_count (ctf_dict_t *, ctf_id_t);

/* Iterators.  */

/* ctf_member_next is a _next-style iterator that can additionally traverse into
   the members of unnamed structs nested within this struct as if they were
   direct members, if CTF_MN_RECURSE is passed in the flags.  */

extern int ctf_member_iter (ctf_dict_t *, ctf_id_t, ctf_member_f *, void *);
extern ssize_t ctf_member_next (ctf_dict_t *, ctf_id_t, ctf_next_t **,
				const char **name, ctf_id_t *membtype,
				int flags);

/* Return all enumeration constants in a given enum type.  */
extern int ctf_enum_iter (ctf_dict_t *, ctf_id_t, ctf_enum_f *, void *);
extern const char *ctf_enum_next (ctf_dict_t *, ctf_id_t, ctf_next_t **,
				  int *);

/* Return all enumeration constants with a given name in a given dict, similar
   to ctf_lookup_enumerator above but capable of returning multiple values.
   Enumerators in parent dictionaries are not returned: enumerators in non-root
   types *are* returned.  This operation internally iterates over all types in
   the dict, so is relatively expensive in large dictionaries.

   There is nothing preventing NAME from being changed by the caller in the
   middle of iteration: the results might be slightly confusing, but they are
   well-defined.  */

extern ctf_id_t ctf_lookup_enumerator_next (ctf_dict_t *, const char *name,
					    ctf_next_t **, int64_t *enum_value);

/* Likewise, across all dicts in an archive (parent first).  The DICT and ERRP
   arguments are not optional: without the forer you can't tell which dict the
   returned type is in, and without the latter you can't distinguish real errors
   from end-of-iteration.  DICT should be NULL before the first call and is set
   to NULL after the last and on error: on successful call it is set to the dict
   containing the returned enum, and it is the caller's responsibility to
   ctf_dict_close() it.  The caller should otherwise pass it back in unchanged
   (do not reassign it during iteration, just as with the ctf_next_t iterator
   itself).  */

extern ctf_id_t ctf_arc_lookup_enumerator_next (ctf_archive_t *, const char *name,
						ctf_next_t **, int64_t *enum_value,
						ctf_dict_t **dict, int *errp);

/* Iterate over all types in a dict.  ctf_type_iter_all recurses over all types:
   ctf_type_iter recurses only over types with user-visible names (for which
   CTF_ADD_ROOT was passed).  All such types are returned, even if they are
   things like pointers that intrinsically have no name: this is the only effect
   of CTF_ADD_ROOT for such types.  ctf_type_next allows you to choose whether
   to see non-root types or not with the want_hidden arg: if set, the flag (if
   passed) returns the non-root state of each type in turn.  Types in parent
   dictionaries are not returned.  */

extern int ctf_type_iter (ctf_dict_t *, ctf_type_f *, void *);
extern int ctf_type_iter_all (ctf_dict_t *, ctf_type_all_f *, void *);
extern ctf_id_t ctf_type_next (ctf_dict_t *, ctf_next_t **,
			       int *flag, int want_hidden);

extern int ctf_variable_iter (ctf_dict_t *, ctf_variable_f *, void *);
extern ctf_id_t ctf_variable_next (ctf_dict_t *, ctf_next_t **,
				   const char **);

/* ctf_archive_iter and ctf_archive_next open each member dict for you,
   automatically importing any parent dict as usual: ctf_archive_iter closes the
   dict on return from ctf_archive_member_f, but for ctf_archive_next the caller
   must close each dict returned.  If skip_parent is set, the parent dict is
   skipped on the basis that it's already been seen in every child dict (but if
   no child dicts exist, this will lead to nothing being returned).

   If an open fails, ctf_archive_iter returns -1 early (losing the error), but
   ctf_archive_next both passes back the error in the passed errp and allows you
   to iterate past errors (until the usual ECTF_NEXT_END is returned).  */

extern int ctf_archive_iter (const ctf_archive_t *, ctf_archive_member_f *,
			     void *);
extern ctf_dict_t *ctf_archive_next (const ctf_archive_t *, ctf_next_t **,
				     const char **, int skip_parent, int *errp);

/* Pass the raw content of each archive member in turn to
   ctf_archive_raw_member_f.

   This function alone does not currently operate on CTF files masquerading as
   archives, and returns -EINVAL: the raw data is no longer available.  It is
   expected to be used only by archiving tools, in any case, which have no need
   to deal with non-archives at all.  (There is currently no _next analogue of
   this function.)  */

extern int ctf_archive_raw_iter (const ctf_archive_t *,
				 ctf_archive_raw_member_f *, void *);

/* Dump the contents of a section in a CTF dict.  STATE is an
   iterator which should be a pointer to a variable set to NULL.  The decorator
   is called with each line in turn and can modify it or allocate and return a
   new one.  ctf_dump accumulates all the results and returns a single giant
   multiline string.  */

extern char *ctf_dump (ctf_dict_t *, ctf_dump_state_t **state,
		       ctf_sect_names_t sect, ctf_dump_decorate_f *,
		       void *arg);

/* Error-warning reporting: an 'iterator' that returns errors and warnings from
   the error/warning list, in order of emission.  Errors and warnings are popped
   after return: the caller must free the returned error-text pointer.  */
extern char *ctf_errwarning_next (ctf_dict_t *, ctf_next_t **,
				  int *is_warning, int *errp);

/* Creation.  */

/* Create a new, empty dict.  If creation fails, return NULL and put a CTF error
   code in the passed-in int (if set).  */
extern ctf_dict_t *ctf_create (int *);

/* Add specific types to a dict.  You can add new types to any dict, but you can
   only add members to types that have been added since this dict was read in
   (you cannot read in a dict, look up a type in it, then add members to
   it).  All adding functions take a uint32_t CTF_ADD_ROOT / CTF_ADD_NONROOT
   flag to indicate whether this type should be visible to name lookups via
   ctf_lookup_by_name et al.  */

extern ctf_id_t ctf_add_array (ctf_dict_t *, uint32_t,
			       const ctf_arinfo_t *);
extern ctf_id_t ctf_add_const (ctf_dict_t *, uint32_t, ctf_id_t);
extern ctf_id_t ctf_add_enum_encoded (ctf_dict_t *, uint32_t, const char *,
				      const ctf_encoding_t *);
extern ctf_id_t ctf_add_enum (ctf_dict_t *, uint32_t, const char *);
extern ctf_id_t ctf_add_float (ctf_dict_t *, uint32_t,
			       const char *, const ctf_encoding_t *);
extern ctf_id_t ctf_add_forward (ctf_dict_t *, uint32_t, const char *,
				 uint32_t);
extern ctf_id_t ctf_add_function (ctf_dict_t *, uint32_t,
				  const ctf_funcinfo_t *, const ctf_id_t *);
extern ctf_id_t ctf_add_integer (ctf_dict_t *, uint32_t, const char *,
				 const ctf_encoding_t *);

/* Add a "slice", which wraps some integral type and changes its encoding
   (useful for bitfields, etc).  In most respects slices are treated the same
   kind as the type they wrap: only ctf_type_reference can see the difference,
   returning the wrapped type.  */

extern ctf_id_t ctf_add_slice (ctf_dict_t *, uint32_t, ctf_id_t, const ctf_encoding_t *);
extern ctf_id_t ctf_add_pointer (ctf_dict_t *, uint32_t, ctf_id_t);
extern ctf_id_t ctf_add_type (ctf_dict_t *, ctf_dict_t *, ctf_id_t);
extern ctf_id_t ctf_add_typedef (ctf_dict_t *, uint32_t, const char *,
				 ctf_id_t);
extern ctf_id_t ctf_add_restrict (ctf_dict_t *, uint32_t, ctf_id_t);

/* Struct and union addition.  Straight addition uses possibly-confusing rules
   to guess the final size of the struct/union given its members: to explicitly
   state the size of the struct or union (to report compiler-generated padding,
   etc) use the _sized variants.  */

extern ctf_id_t ctf_add_struct (ctf_dict_t *, uint32_t, const char *);
extern ctf_id_t ctf_add_union (ctf_dict_t *, uint32_t, const char *);
extern ctf_id_t ctf_add_struct_sized (ctf_dict_t *, uint32_t, const char *,
				      size_t);
extern ctf_id_t ctf_add_union_sized (ctf_dict_t *, uint32_t, const char *,
				     size_t);

/* Note that CTF cannot encode a given type.  This usually returns an
   ECTF_NONREPRESENTABLE error when queried.  Mostly useful for struct members,
   variables, etc, to point to.  */

extern ctf_id_t ctf_add_unknown (ctf_dict_t *, uint32_t, const char *);
extern ctf_id_t ctf_add_volatile (ctf_dict_t *, uint32_t, ctf_id_t);

/* Add an enumerator to an enum.  If the enum is non-root, so are all the
   constants added to it by ctf_add_enumerator.  */

extern int ctf_add_enumerator (ctf_dict_t *, ctf_id_t, const char *, int);

/* Add a member to a struct or union, either at the next available offset (with
   suitable padding for the alignment) or at a specific offset, and possibly
   with a specific encoding (creating a slice for you).  Offsets need not be
   unique, and need not be added in ascending order.  */

extern int ctf_add_member (ctf_dict_t *, ctf_id_t, const char *, ctf_id_t);
extern int ctf_add_member_offset (ctf_dict_t *, ctf_id_t, const char *,
				  ctf_id_t, unsigned long);
extern int ctf_add_member_encoded (ctf_dict_t *, ctf_id_t, const char *,
				   ctf_id_t, unsigned long,
				   const ctf_encoding_t);

extern int ctf_add_variable (ctf_dict_t *, const char *, ctf_id_t);

/* Set the size and member and index types of an array.  */

extern int ctf_set_array (ctf_dict_t *, ctf_id_t, const ctf_arinfo_t *);

/* Add a function oor object symbol type with a particular name, without saying
   anything about the actual symbol index.  (The linker will then associate them
   with actual symbol indexes using the ctf_link functions below.)  */

extern int ctf_add_objt_sym (ctf_dict_t *, const char *, ctf_id_t);
extern int ctf_add_func_sym (ctf_dict_t *, const char *, ctf_id_t);

/* Snapshot/rollback.  Call ctf_update to snapshot the state of a dict:
  a later call to ctf_discard then deletes all types added since (but not new
  members, enumerands etc).  Call ctf_snapshot to return a snapshot ID: pass
  one of these IDs to ctf_rollback to discard all types added since the
  corresponding call to ctf_snapshot.  */

extern int ctf_update (ctf_dict_t *);
extern ctf_snapshot_id_t ctf_snapshot (ctf_dict_t *);
extern int ctf_rollback (ctf_dict_t *, ctf_snapshot_id_t);
extern int ctf_discard (ctf_dict_t *);

/* Dict writeout.

   ctf_write: write out an uncompressed dict to an fd.
   ctf_compress_write: write out a compressed dict to an fd (currently always
   gzip, but this may change in future).
   ctf_write_mem: write out a dict to a buffer and return it and its size,
   compressing it if its uncompressed size is over THRESHOLD.  */

extern int ctf_write (ctf_dict_t *, int);
extern int ctf_compress_write (ctf_dict_t * fp, int fd);
extern unsigned char *ctf_write_mem (ctf_dict_t *, size_t *, size_t threshold);

/* Create a CTF archive named FILE from CTF_DICTS inputs with NAMES (or write it
   to the passed-in fd).  */

extern int ctf_arc_write (const char *file, ctf_dict_t **ctf_dicts, size_t,
			  const char **names, size_t);
extern int ctf_arc_write_fd (int, ctf_dict_t **, size_t, const char **,
			     size_t);

/* Linking.  These functions are used by ld to link .ctf sections in input
   object files into a single .ctf section which is an archive possibly
   containing members containing types whose names collide across multiple
   compilation units, but they are usable by other programs as well and are not
   private to the linker.  */

/* Add a CTF archive to the link with a given NAME (usually the name of the
   containing object file).  The dict added to is usually a new dict created
   with ctf_create which will be filled with types corresponding to the shared
   dict in the output (conflicting types in child dicts in the output archive
   are stored in internal space inside this dict, but are not easily visible
   until after ctf_link_write below).

   The NAME need not be unique (but usually is).  */

extern int ctf_link_add_ctf (ctf_dict_t *, ctf_archive_t *, const char *name);

/* Do the deduplicating link, filling the dict with types.  The FLAGS are the
   CTF_LINK_* flags above.  */

extern int ctf_link (ctf_dict_t *, int flags);

/* Symtab linker handling, called after ctf_link to set up the symbol type
   information used by ctf_*_lookup_symbol.  */

/* Add strings to the link from the ELF string table, repeatedly calling
   ADD_STRING to add each string and its corresponding offset in turn.  */

typedef const char *ctf_link_strtab_string_f (uint32_t *offset, void *arg);
extern int ctf_link_add_strtab (ctf_dict_t *,
				ctf_link_strtab_string_f *add_string, void *);

/* Note that a given symbol will be public with a given set of properties.
   If the symbol has been added with that name via ctf_add_{func,objt}_sym,
   this symbol type will end up in the symtypetabs and can be looked up via
   ctf_*_lookup_symbol after the dict is read back in.  */

extern int ctf_link_add_linker_symbol (ctf_dict_t *, ctf_link_sym_t *);

/* Impose an ordering on symbols, as defined by the strtab and symbol
   added by earlier calls to the above two functions.  */

extern int ctf_link_shuffle_syms (ctf_dict_t *);

/* Return the serialized form of this ctf_linked dict as a new
   dynamically-allocated string, compressed if size over THRESHOLD.

   May be a CTF dict or a CTF archive (this library mostly papers over the
   differences so you can open both the same way, treat both as ctf_archive_t
   and so on).  */

extern unsigned char *ctf_link_write (ctf_dict_t *, size_t *size,
				      size_t threshold);

/* Specialist linker functions.  These functions are not used by ld, but can be
   used by other programs making use of the linker machinery for other purposes
   to customize its output.  Must be called befoore ctf_link. */

/* Add an entry to rename a given compilation unit to some other name.  This
   is only used if conflicting types are found in that compilation unit: they
   will instead be placed in the child dict named TO. Many FROMs can map to one
   TO: all the types are placed together in that dict, with any whose names
   collide as a result being marked as non-root types.  */

extern int ctf_link_add_cu_mapping (ctf_dict_t *, const char *from,
				    const char *to);

/* Allow CTF archive names to be tweaked at the last minute before writeout.
   Unlike cu-mappings, this cannot transform names so that they collide: it's
   meant for unusual use cases that use names for archive members that are not
   exactly the same as CU names but are modified in some systematic way.  */
typedef char *ctf_link_memb_name_changer_f (ctf_dict_t *,
					    const char *, void *);
extern void ctf_link_set_memb_name_changer
  (ctf_dict_t *, ctf_link_memb_name_changer_f *, void *);

/* Filter out unwanted variables, which can be very voluminous, and (unlike
   symbols) cause the CTF string table to grow to hold their names.  The
   variable filter should return nonzero if a variable should not appear in the
   output.  */
typedef int ctf_link_variable_filter_f (ctf_dict_t *, const char *, ctf_id_t,
					void *);
extern int ctf_link_set_variable_filter (ctf_dict_t *,
					 ctf_link_variable_filter_f *, void *);

/* Turn debugging off and on, and get its value.  This is the same as setting
   LIBCTF_DEBUG in the environment.  */
extern void ctf_setdebug (int debug);
extern int ctf_getdebug (void);

/* Deprecated aliases for existing functions and types.  */

struct ctf_file;
typedef struct ctf_dict ctf_file_t;
extern void ctf_file_close (ctf_file_t *);
extern ctf_dict_t *ctf_parent_file (ctf_dict_t *);
extern ctf_dict_t *ctf_arc_open_by_name (const ctf_archive_t *,
					 const char *, int *);
extern ctf_dict_t *ctf_arc_open_by_name_sections (const ctf_archive_t *arc,
						  const ctf_sect_t *symsect,
						  const ctf_sect_t *strsect,
						  const char *name, int *errp);

/* Deprecated witeout function to write out a gzip-compressed dict.  Unlike all
   the other writeout functions, this even compresses the header (it has to,
   since it's passed a gzFile), so the caller must also decompress it, since
   ctf_open() etc cannot tell it is a CTF dict or how large it is before
   decompression.  */

extern int ctf_gzwrite (ctf_dict_t *fp, gzFile fd);

/* Deprecated functions with no current use.  */

extern const char *ctf_label_topmost (ctf_dict_t *);
extern int ctf_label_info (ctf_dict_t *, const char *, ctf_lblinfo_t *);
extern int ctf_label_iter (ctf_dict_t *, ctf_label_f *, void *);
extern int ctf_label_next (ctf_dict_t *, ctf_next_t **, const char **); /* TBD */

#ifdef	__cplusplus
}
#endif

#endif				/* _CTF_API_H */
