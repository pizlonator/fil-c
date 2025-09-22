/* Internal interfaces for libelf.
   Copyright (C) 1998-2010, 2015, 2016 Red Hat, Inc.
   Copyright (C) 2023 Mark J. Wielaard <mark@klomp.org>
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#ifndef _LIBELFP_H
#define _LIBELFP_H 1

#include <ar.h>
#include <gelf.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <system.h>

/* Helper Macros to write 32 bit and 64 bit functions.  */
#define __elfw2_(Bits, Name) __elf##Bits##_##Name
#define elfw2_(Bits, Name) elf##Bits##_##Name
#define ElfW2_(Bits, Name) Elf##Bits##_##Name
#define ELFW2_(Bits, Name) ELF##Bits##_##Name
#define ELFW_(Name, Bits) Name##Bits
#define __elfw2(Bits, Name) __elfw2_(Bits, Name)
#define elfw2(Bits, Name) elfw2_(Bits, Name)
#define ElfW2(Bits, Name) ElfW2_(Bits, Name)
#define ELFW2(Bits, Name) ELFW2_(Bits, Name)
#define ELFW(Name, Bits)  ELFW_(Name, Bits)


/* Sizes of the external types, for 32 bits objects.  */
#define ELF32_FSZ_ADDR   4
#define ELF32_FSZ_OFF    4
#define ELF32_FSZ_HALF   2
#define ELF32_FSZ_WORD   4
#define ELF32_FSZ_SWORD  4
#define ELF32_FSZ_XWORD  8
#define ELF32_FSZ_SXWORD 8
#define ELF32_FSZ_RELR   4

/* Same for 64 bits objects.  */
#define ELF64_FSZ_ADDR   8
#define ELF64_FSZ_OFF    8
#define ELF64_FSZ_HALF   2
#define ELF64_FSZ_WORD   4
#define ELF64_FSZ_SWORD  4
#define ELF64_FSZ_XWORD  8
#define ELF64_FSZ_SXWORD 8
#define ELF64_FSZ_RELR   8


/* This is an extension of the ELF_F_* enumeration.  The values here are
   not part of the library interface, they are only used internally.  */
enum
{
  ELF_F_MMAPPED = 0x40,
  ELF_F_MALLOCED = 0x80,
  ELF_F_FILEDATA = 0x100
};


/* Get definition of all the external types.  */
#include "exttypes.h"


/* Error values.  */
enum
{
  ELF_E_NOERROR = 0,
  ELF_E_UNKNOWN_ERROR,
  ELF_E_UNKNOWN_VERSION,
  ELF_E_UNKNOWN_TYPE,
  ELF_E_INVALID_HANDLE,
  ELF_E_SOURCE_SIZE,
  ELF_E_DEST_SIZE,
  ELF_E_INVALID_ENCODING,
  ELF_E_NOMEM,
  ELF_E_INVALID_FILE,
  ELF_E_INVALID_ELF,
  ELF_E_INVALID_OP,
  ELF_E_NO_VERSION,
  ELF_E_INVALID_CMD,
  ELF_E_RANGE,
  ELF_E_ARCHIVE_FMAG,
  ELF_E_INVALID_ARCHIVE,
  ELF_E_NO_ARCHIVE,
  ELF_E_NO_INDEX,
  ELF_E_READ_ERROR,
  ELF_E_WRITE_ERROR,
  ELF_E_INVALID_CLASS,
  ELF_E_INVALID_INDEX,
  ELF_E_INVALID_OPERAND,
  ELF_E_INVALID_SECTION,
  ELF_E_INVALID_COMMAND,
  ELF_E_WRONG_ORDER_EHDR,
  ELF_E_FD_DISABLED,
  ELF_E_FD_MISMATCH,
  ELF_E_OFFSET_RANGE,
  ELF_E_NOT_NUL_SECTION,
  ELF_E_DATA_MISMATCH,
  ELF_E_INVALID_SECTION_HEADER,
  ELF_E_INVALID_DATA,
  ELF_E_DATA_ENCODING,
  ELF_E_SECTION_TOO_SMALL,
  ELF_E_INVALID_ALIGN,
  ELF_E_INVALID_SHENTSIZE,
  ELF_E_UPDATE_RO,
  ELF_E_NOFILE,
  ELF_E_GROUP_NOT_REL,
  ELF_E_INVALID_PHDR,
  ELF_E_NO_PHDR,
  ELF_E_INVALID_OFFSET,
  ELF_E_INVALID_SECTION_TYPE,
  ELF_E_INVALID_SECTION_FLAGS,
  ELF_E_NOT_COMPRESSED,
  ELF_E_ALREADY_COMPRESSED,
  ELF_E_UNKNOWN_COMPRESSION_TYPE,
  ELF_E_COMPRESS_ERROR,
  ELF_E_DECOMPRESS_ERROR,
  /* Keep this as the last entry.  */
  ELF_E_NUM
};


/* The visible `Elf_Data' type is not sufficient for some operations due
   to a misdesigned interface.  Extend it for internal purposes.  */
typedef struct
{
  Elf_Data d;
  Elf_Scn *s;
} Elf_Data_Scn;


/* List of `Elf_Data' descriptors.  This is what makes up the section
   contents.  */
typedef struct Elf_Data_List
{
  /* `data' *must* be the first element in the struct.  */
  Elf_Data_Scn data;
  struct Elf_Data_List *next;
  int flags;
} Elf_Data_List;


/* Descriptor for ELF section.  */
struct Elf_Scn
{
  /* We have to distinguish several different situations:

     1. the section is user created.  Therefore there is no file or memory
        region to read the data from.  Here we have two different subcases:

        a) data was not yet added (before the first `elf_newdata' call)

        b) at least one data set is available

     2. this is a section from a file/memory region.  We have to read the
        current content in one data block if we have to.  But we don't
        read the data until it is necessary.  So we have the subcases:

        a) the section in the file has size zero (for whatever reason)

        b) the data of the file is not (yet) read

        c) the data is read and available.

     In addition to this we have different data sets, the raw and the converted
     data.  This distinction only exists for the data read from the file.
     All user-added data set (all but the first when read from the file or
     all of them for user-create sections) are the same in both formats.
     We don't create the converted data before it is necessary.

     The `data_read' element signals whether data is available in the
     raw format.

     If there is data from the file/memory region or if read one data
     set is added the `rawdata_list_read' pointer in non-NULL and points
     to the last filled data set.  `raw_datalist_rear' is therefore NULL
     only if there is no data set at all.

     This so far allows to distinguish all but two cases (given that the
     `rawdata_list' and `data_list' entries are initialized to zero) is
     between not yet loaded data from the file/memory region and a section
     with zero size and type ELF_T_BYTE.   */
  Elf_Data_List data_list;	/* List of data buffers.  */
  Elf_Data_List *data_list_rear; /* Pointer to the rear of the data list. */

  Elf_Data_Scn rawdata;		/* Uninterpreted data of the section.  */

  int data_read;		/* Nonzero if the section was created by the
				   user or if the data from the file/memory
				   is read.  */
  int shndx_index;		/* Index of the extended section index
				   table for this symbol table (if this
				   section is a symbol table).  */

  size_t index;			/* Index of this section.  */
  struct Elf *elf;		/* The underlying ELF file.  */

  union
  {
    Elf32_Shdr *e32;		/* Pointer to 32bit section header.  */
    Elf64_Shdr *e64;		/* Pointer to 64bit section header.  */
  } shdr;

  unsigned int shdr_flags;	/* Section header modified?  */
  unsigned int flags;		/* Section changed in size?
				   ELF_F_MALLOCED for a Elf_Data_Chunk
				   dummy_scn means the rawchunks
				   data.d.d_buf was malloced. For normal
				   sections it means rawdata_base was
				   malloced (by elf_compress) even if
				   the Elf was mmapped.  */

  char *rawdata_base;		/* The unmodified data of the section.  */
  char *data_base;		/* The converted data of the section.  */

  char *zdata_base;		/* The uncompressed data of the section.  */
  size_t zdata_size;		/* If zdata_base != NULL, the size of data.  */
  size_t zdata_align;		/* If zdata_base != NULL, the addralign.  */

  struct Elf_ScnList *list;	/* Pointer to the section list element the
				   data is in.  */
};


/* List of section.  */
typedef struct Elf_ScnList
{
  unsigned int cnt;		/* Number of elements of 'data' used.  */
  unsigned int max;		/* Number of elements of 'data' allocated.  */
  struct Elf_ScnList *next;	/* Next block of sections.  */
  struct Elf_Scn data[0];	/* Section data.  */
} Elf_ScnList;


/* elf_getdata_rawchunk result.  */
typedef struct Elf_Data_Chunk
{
  Elf_Data_Scn data;
  Elf_Scn dummy_scn;
  int64_t offset;		/* The original raw offset in the Elf image.  */
} Elf_Data_Chunk;


/* The ELF descriptor.  */
struct Elf
{
  /* Address to which the file was mapped.  NULL if not mapped.  */
  void *map_address;

  /* When created for an archive member this points to the descriptor
     for the archive. */
  Elf *parent;
  Elf *next;             /* Used in list of archive descriptors.  */

  /* What kind of file is underneath (ELF file, archive...).  */
  Elf_Kind kind;

  /* Command used to create this descriptor.  */
  Elf_Cmd cmd;

  /* The binary class.  */
  unsigned int class;

  /* The used file descriptor.  -1 if not available anymore.  */
  int fildes;

  /* Offset in the archive this file starts or zero.  */
  int64_t start_offset;

  /* Size of the file in the archive or the entire file size, or ~0
     for an (yet) unknown size.  */
  size_t maximum_size;

  /* Describes the way the memory was allocated and if the dirty bit is
     signalled it means that the whole file has to be rewritten since
     the layout changed.  */
  int flags;

  /* Reference counting for the descriptor.  */
  int ref_count;

  /* Lock to handle multithreaded programs.  */
  rwlock_define (,lock);

  union
  {
    struct
    {
      /* The next fields are only useful when testing for ==/!= NULL.  */
      void *ehdr;
      void *shdr;
      void *phdr;

      Elf_ScnList *scns_last;	/* Last element in the section list.
				   If NULL the data has not yet been
				   read from the file.  */
      void *rawchunks;		/* Tree of elf_getdata_rawchunk results.  */
      unsigned int scnincr;	/* Number of sections allocate the last
				   time.  */
      int ehdr_flags;		/* Flags (dirty) for ELF header.  */
      int phdr_flags;		/* Flags (dirty|malloc) for program header.  */
      int shdr_malloced;	/* Nonzero if shdr array was allocated.  */
      off_t sizestr_offset;	/* Offset of the size string in the parent
				   if this is an archive member.  */
    } elf;

    struct
    {
      Elf32_Ehdr *ehdr;		/* Pointer to the ELF header.  This is
				   never malloced.  */
      Elf32_Shdr *shdr;		/* Used when reading from a file.  */
      Elf32_Phdr *phdr;		/* Pointer to the program header array.  */
      Elf_ScnList *scns_last;	/* Last element in the section list.
				   If NULL the data has not yet been
				   read from the file.  */
      void *rawchunks;		/* Tree of elf_getdata_rawchunk results.  */
      unsigned int scnincr;	/* Number of sections allocate the last
				   time.  */
      int ehdr_flags;		/* Flags (dirty) for ELF header.  */
      int phdr_flags;		/* Flags (dirty|malloc) for program header.  */
      int shdr_malloced;	/* Nonzero if shdr array was allocated.  */
      int64_t sizestr_offset;	/* Offset of the size string in the parent
				   if this is an archive member.  */
      Elf32_Ehdr ehdr_mem;	/* Memory used for ELF header when not
				   mmaped.  */
      char __e32scnspad[sizeof (Elf64_Ehdr) - sizeof (Elf32_Ehdr)];

      /* The section array.  */
      Elf_ScnList scns;
    } elf32;

    struct
    {
      Elf64_Ehdr *ehdr;		/* Pointer to the ELF header.  This is
				   never malloced.  */
      Elf64_Shdr *shdr;		/* Used when reading from a file.  */
      Elf64_Phdr *phdr;		/* Pointer to the program header array.  */
      Elf_ScnList *scns_last;	/* Last element in the section list.
				   If NULL the data has not yet been
				   read from the file.  */
      void *rawchunks;		/* Tree of elf_getdata_rawchunk results.  */
      unsigned int scnincr;	/* Number of sections allocate the last
				   time.  */
      int ehdr_flags;		/* Flags (dirty) for ELF header.  */
      int phdr_flags;		/* Flags (dirty|malloc) for program header.  */
      int shdr_malloced;	/* Nonzero if shdr array was allocated.  */
      int64_t sizestr_offset;	/* Offset of the size string in the parent
				   if this is an archive member.  */
      Elf64_Ehdr ehdr_mem;	/* Memory used for ELF header when not
				   mmaped.  */

      /* The section array.  */
      Elf_ScnList scns;
    } elf64;

    struct
    {
      Elf *children;		/* List of all descriptors for this archive. */
      Elf_Arsym *ar_sym;	/* Symbol table returned by elf_getarsym.  */
      size_t ar_sym_num;	/* Number of entries in `ar_sym'.  */
      char *long_names;		/* If no index is available but long names
				   are used this elements points to the data.*/
      size_t long_names_len;	/* Length of the long name table.  */
      int64_t offset;		/* Offset in file we are currently at.
				   elf_next() advances this to the next
				   member of the archive.  */
      Elf_Arhdr elf_ar_hdr;	/* Structure returned by 'elf_getarhdr'.  */
      struct ar_hdr ar_hdr;	/* Header read from file.  */
      char ar_name[16];		/* NUL terminated ar_name of elf_ar_hdr.  */
      char raw_name[17];	/* This is a buffer for the NUL terminated
				   named raw_name used in the elf_ar_hdr.  */
    } ar;
  } state;

  /* There absolutely never must be anything following the union.  */
};

/* Type of the conversion functions.  These functions will convert the
   byte order.  */
typedef void (*xfct_t) (void *, const void *, size_t, int);

/* The table with the function pointers.  */
extern const xfct_t __elf_xfctstom[ELFCLASSNUM - 1][ELF_T_NUM]
  attribute_hidden;


/* Array with sizes of the external types indexed by ELF version, binary
   class, and type. */
extern const size_t __libelf_type_sizes[ELFCLASSNUM - 1][ELF_T_NUM]
  attribute_hidden;
/* We often have to access the size for a type in the current version.  */
# define elf_typesize(class,type,n) \
  (__libelf_type_sizes[ELFW(ELFCLASS,class) - 1][type] * n)

/* The byte value used for filling gaps.  */
extern int __libelf_fill_byte attribute_hidden;

/* EV_CURRENT if the version was set, EV_NONE otherwise.  */
extern unsigned int __libelf_version attribute_hidden;

/* Array with alignment requirements of the internal types indexed by
   binary class, and type. */
extern const uint_fast8_t __libelf_type_aligns[ELFCLASSNUM - 1][ELF_T_NUM]
  attribute_hidden;
# define __libelf_type_align(class, type)	\
    (__libelf_type_aligns[class - 1][type] ?: 1)

/* Given an GElf_Ehdr handle and a section type returns the Elf_Data d_type.
   Should not be called when SHF_COMPRESSED is set, the d_type should
   be ELF_T_BYTE.  */
extern Elf_Type __libelf_data_type (GElf_Ehdr *ehdr,
				    int sh_type, GElf_Xword align)
  internal_function;


/* Create Elf descriptor from memory image.  */
extern Elf *__libelf_read_mmaped_file (int fildes, void *map_address,
				       int64_t offset, size_t maxsize,
				       Elf_Cmd cmd, Elf *parent)
     internal_function;

/* Set error value.  */
extern void __libelf_seterrno (int value) internal_function;

/* Get the next archive header.  */
extern int __libelf_next_arhdr_wrlock (Elf *elf) internal_function;

/* Read all of the file associated with the descriptor.  */
extern char *__libelf_readall (Elf *elf) internal_function;

/* Read the complete section table and convert the byte order if necessary.  */
extern int __libelf_readsections (Elf *elf) internal_function;

/* Store the information for the raw data in the `rawdata_list' element.  */
extern int __libelf_set_rawdata (Elf_Scn *scn) internal_function;
extern int __libelf_set_rawdata_wrlock (Elf_Scn *scn) internal_function;


/* Helper functions for elf_update.  */
extern int64_t __elf32_updatenull_wrlock (Elf *elf, int *change_bop,
					  size_t shnum) internal_function;
extern int64_t __elf64_updatenull_wrlock (Elf *elf, int *change_bop,
					  size_t shnum) internal_function;

extern int __elf32_updatemmap (Elf *elf, int change_bo, size_t shnum)
     internal_function;
extern int __elf64_updatemmap (Elf *elf, int change_bo, size_t shnum)
     internal_function;
extern int __elf32_updatefile (Elf *elf, int change_bo, size_t shnum)
     internal_function;
extern int __elf64_updatefile (Elf *elf, int change_bo, size_t shnum)
     internal_function;


/* Alias for exported functions to avoid PLT entries, and
   rdlock/wrlock variants of these functions.  */
extern int __elf_end_internal (Elf *__elf) attribute_hidden;
extern Elf *__elf_begin_internal (int __fildes, Elf_Cmd __cmd, Elf *__ref)
     attribute_hidden;
extern Elf32_Ehdr *__elf32_getehdr_wrlock (Elf *__elf) internal_function;
extern Elf64_Ehdr *__elf64_getehdr_wrlock (Elf *__elf) internal_function;
extern Elf32_Ehdr *__elf32_newehdr_internal (Elf *__elf) attribute_hidden;
extern Elf64_Ehdr *__elf64_newehdr_internal (Elf *__elf) attribute_hidden;
extern Elf32_Phdr *__elf32_getphdr_internal (Elf *__elf) attribute_hidden;
extern Elf64_Phdr *__elf64_getphdr_internal (Elf *__elf) attribute_hidden;
extern Elf32_Phdr *__elf32_getphdr_wrlock (Elf *__elf) attribute_hidden;
extern Elf64_Phdr *__elf64_getphdr_wrlock (Elf *__elf) attribute_hidden;
extern Elf32_Phdr *__elf32_newphdr_internal (Elf *__elf, size_t __cnt)
     attribute_hidden;
extern Elf64_Phdr *__elf64_newphdr_internal (Elf *__elf, size_t __cnt)
     attribute_hidden;
extern Elf_Scn *__elf32_offscn_internal (Elf *__elf, Elf32_Off __offset)
     attribute_hidden;
extern Elf_Scn *__elf64_offscn_internal (Elf *__elf, Elf64_Off __offset)
     attribute_hidden;
extern int __elf_getphdrnum_rdlock (Elf *__elf, size_t *__dst)
     internal_function;
extern int __elf_getphdrnum_chk_rdlock (Elf *__elf, size_t *__dst)
     internal_function;
extern int __elf_getshdrnum_rdlock (Elf *__elf, size_t *__dst)
     internal_function;
extern int __elf_getshdrstrndx_internal (Elf *__elf, size_t *__dst)
     attribute_hidden;
extern Elf32_Shdr *__elf32_getshdr_rdlock (Elf_Scn *__scn) internal_function;
extern Elf64_Shdr *__elf64_getshdr_rdlock (Elf_Scn *__scn) internal_function;
extern Elf32_Shdr *__elf32_getshdr_wrlock (Elf_Scn *__scn) internal_function;
extern Elf64_Shdr *__elf64_getshdr_wrlock (Elf_Scn *__scn) internal_function;
extern Elf32_Chdr *__elf32_getchdr_wrlock (Elf_Scn *__scn) internal_function;
extern Elf64_Chdr *__elf64_getchdr_wrlock (Elf_Scn *__scn) internal_function;
extern Elf_Scn *__elf_getscn_internal (Elf *__elf, size_t __index)
     attribute_hidden;
extern Elf_Scn *__elf_nextscn_internal (Elf *__elf, Elf_Scn *__scn)
     attribute_hidden;
extern int __elf_scnshndx_internal (Elf_Scn *__scn) attribute_hidden;
extern Elf_Data *__elf_getdata_internal (Elf_Scn *__scn, Elf_Data *__data)
     attribute_hidden;
extern Elf_Data *__elf_getdata_rdlock (Elf_Scn *__scn, Elf_Data *__data)
     internal_function;
extern Elf_Data *__elf_getdata_wrlock (Elf_Scn *__scn, Elf_Data *__data)
     internal_function;
extern Elf_Data *__elf_rawdata_internal (Elf_Scn *__scn, Elf_Data *__data)
     attribute_hidden;
/* Should be called to setup first section data element if
   data_list_rear is NULL and we know data_read is set and there is
   raw data available.  Might upgrade the ELF lock from a read to a
   write lock.  If the lock is already a write lock set wrlocked.  */
extern void __libelf_set_data_list_rdlock (Elf_Scn *scn, int wrlocked)
     internal_function;
extern char *__elf_strptr_internal (Elf *__elf, size_t __index,
				    size_t __offset) attribute_hidden;
extern Elf_Data *__elf32_xlatetom_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden;
extern Elf_Data *__elf64_xlatetom_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden;
extern Elf_Data *__elf32_xlatetof_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden;
extern Elf_Data *__elf64_xlatetof_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden;
extern unsigned int __elf_version_internal (unsigned int __version)
     attribute_hidden;
extern unsigned long int __elf_hash_internal (const char *__string)
       __attribute__ ((__pure__)) attribute_hidden;
extern long int __elf32_checksum_internal (Elf *__elf) attribute_hidden;
extern long int __elf64_checksum_internal (Elf *__elf) attribute_hidden;


extern GElf_Ehdr *__gelf_getehdr_rdlock (Elf *__elf, GElf_Ehdr *__dest)
     internal_function;
extern size_t __gelf_fsize_internal (Elf *__elf, Elf_Type __type,
				     size_t __count, unsigned int __version)
     attribute_hidden;
extern GElf_Shdr *__gelf_getshdr_internal (Elf_Scn *__scn, GElf_Shdr *__dst)
     attribute_hidden;
extern GElf_Sym *__gelf_getsym_internal (Elf_Data *__data, int __ndx,
					 GElf_Sym *__dst) attribute_hidden;


extern uint32_t __libelf_crc32 (uint32_t crc, unsigned char *buf, size_t len)
     attribute_hidden;

extern void * __libelf_compress (Elf_Scn *scn, size_t hsize, int ei_data,
				 size_t *orig_size, size_t *orig_addralign,
				 size_t *size, bool force, bool use_zstd)
     internal_function;

extern void * __libelf_decompress (int chtype, void *buf_in, size_t size_in,
				   size_t size_out) internal_function;
extern void * __libelf_decompress_elf (Elf_Scn *scn,
				       size_t *size_out, size_t *addralign)
     internal_function;


extern void __libelf_reset_rawdata (Elf_Scn *scn, void *buf, size_t size,
				    size_t align, Elf_Type type)
     internal_function;


/* We often have to update a flag iff a value changed.  Make this
   convenient.  */
#define update_if_changed(var, exp, flag) \
  do {									      \
    __typeof__ (var) *_var = &(var);					      \
    __typeof__ (exp) _exp = (exp);					      \
    if (*_var != _exp)							      \
      {									      \
	*_var = _exp;							      \
	(flag) |= ELF_F_DIRTY;						      \
      }									      \
  } while (0)

/* Align offset to 4 bytes as needed for note name and descriptor data.
   This is almost always used, except for GNU Property notes, which use
   8 byte padding...  */
#define NOTE_ALIGN4(n)	(((n) + 3) & -4UL)

/* Special note padding rule for GNU Property notes.  */
#define NOTE_ALIGN8(n)	(((n) + 7) & -8UL)

/* Convenience macro.  */
#define INVALID_NDX(ndx, type, data) \
  unlikely ((data)->d_size / sizeof (type) <= (unsigned int) (ndx))

#endif  /* libelfP.h */
