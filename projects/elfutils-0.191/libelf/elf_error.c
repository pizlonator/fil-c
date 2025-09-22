/* Error handling in libelf.
   Copyright (C) 1998-2010, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "libelfP.h"


/* The error number.  */
static __thread int global_error;


int
elf_errno (void)
{
  int result = global_error;
  global_error = ELF_E_NOERROR;
  return result;
}


/* Return the appropriate message for the error.  */
static const char msgstr[] =
{
#define ELF_E_NOERROR_IDX 0
  N_("no error")
  "\0"
#define ELF_E_UNKNOWN_ERROR_IDX (ELF_E_NOERROR_IDX + sizeof "no error")
  N_("unknown error")
  "\0"
#define ELF_E_UNKNOWN_VERSION_IDX \
  (ELF_E_UNKNOWN_ERROR_IDX + sizeof "unknown error")
  N_("unknown version")
  "\0"
#define ELF_E_UNKNOWN_TYPE_IDX \
  (ELF_E_UNKNOWN_VERSION_IDX + sizeof "unknown version")
  N_("unknown type")
  "\0"
#define ELF_E_INVALID_HANDLE_IDX \
  (ELF_E_UNKNOWN_TYPE_IDX + sizeof "unknown type")
  N_("invalid `Elf' handle")
  "\0"
#define ELF_E_SOURCE_SIZE_IDX \
  (ELF_E_INVALID_HANDLE_IDX + sizeof "invalid `Elf' handle")
  N_("invalid size of source operand")
  "\0"
#define ELF_E_DEST_SIZE_IDX \
  (ELF_E_SOURCE_SIZE_IDX + sizeof "invalid size of source operand")
  N_("invalid size of destination operand")
  "\0"
#define ELF_E_INVALID_ENCODING_IDX \
  (ELF_E_DEST_SIZE_IDX + sizeof "invalid size of destination operand")
  N_("invalid encoding")
  "\0"
#define ELF_E_NOMEM_IDX \
  (ELF_E_INVALID_ENCODING_IDX + sizeof "invalid encoding")
  N_("out of memory")
  "\0"
#define ELF_E_INVALID_FILE_IDX \
  (ELF_E_NOMEM_IDX + sizeof "out of memory")
  N_("invalid file descriptor")
  "\0"
#define ELF_E_INVALID_ELF_IDX \
  (ELF_E_INVALID_FILE_IDX + sizeof "invalid file descriptor")
  N_("invalid ELF file data")
  "\0"
#define ELF_E_INVALID_OP_IDX \
  (ELF_E_INVALID_ELF_IDX + sizeof "invalid ELF file data")
  N_("invalid operation")
  "\0"
#define ELF_E_NO_VERSION_IDX \
  (ELF_E_INVALID_OP_IDX + sizeof "invalid operation")
  N_("ELF version not set")
  "\0"
#define ELF_E_INVALID_CMD_IDX \
  (ELF_E_NO_VERSION_IDX + sizeof "ELF version not set")
  N_("invalid command")
  "\0"
#define ELF_E_RANGE_IDX \
  (ELF_E_INVALID_CMD_IDX + sizeof "invalid command")
  N_("offset out of range")
  "\0"
#define ELF_E_ARCHIVE_FMAG_IDX \
  (ELF_E_RANGE_IDX + sizeof "offset out of range")
  N_("invalid fmag field in archive header")
  "\0"
#define ELF_E_INVALID_ARCHIVE_IDX \
  (ELF_E_ARCHIVE_FMAG_IDX + sizeof "invalid fmag field in archive header")
  N_("invalid archive file")
  "\0"
#define ELF_E_NO_ARCHIVE_IDX \
  (ELF_E_INVALID_ARCHIVE_IDX + sizeof "invalid archive file")
  N_("descriptor is not for an archive")
  "\0"
#define ELF_E_NO_INDEX_IDX \
  (ELF_E_NO_ARCHIVE_IDX + sizeof "descriptor is not for an archive")
  N_("no index available")
  "\0"
#define ELF_E_READ_ERROR_IDX \
  (ELF_E_NO_INDEX_IDX + sizeof "no index available")
  N_("cannot read data from file")
  "\0"
#define ELF_E_WRITE_ERROR_IDX \
  (ELF_E_READ_ERROR_IDX + sizeof "cannot read data from file")
  N_("cannot write data to file")
  "\0"
#define ELF_E_INVALID_CLASS_IDX \
  (ELF_E_WRITE_ERROR_IDX + sizeof "cannot write data to file")
  N_("invalid binary class")
  "\0"
#define ELF_E_INVALID_INDEX_IDX \
  (ELF_E_INVALID_CLASS_IDX + sizeof "invalid binary class")
  N_("invalid section index")
  "\0"
#define ELF_E_INVALID_OPERAND_IDX \
  (ELF_E_INVALID_INDEX_IDX + sizeof "invalid section index")
  N_("invalid operand")
  "\0"
#define ELF_E_INVALID_SECTION_IDX \
  (ELF_E_INVALID_OPERAND_IDX + sizeof "invalid operand")
  N_("invalid section")
  "\0"
#define ELF_E_INVALID_COMMAND_IDX \
  (ELF_E_INVALID_SECTION_IDX + sizeof "invalid section")
  N_("invalid command")
  "\0"
#define ELF_E_WRONG_ORDER_EHDR_IDX \
  (ELF_E_INVALID_COMMAND_IDX + sizeof "invalid command")
  N_("executable header not created first")
  "\0"
#define ELF_E_FD_DISABLED_IDX \
  (ELF_E_WRONG_ORDER_EHDR_IDX + sizeof "executable header not created first")
  N_("file descriptor disabled")
  "\0"
#define ELF_E_FD_MISMATCH_IDX \
  (ELF_E_FD_DISABLED_IDX + sizeof "file descriptor disabled")
  N_("archive/member file descriptor mismatch")
  "\0"
#define ELF_E_OFFSET_RANGE_IDX \
  (ELF_E_FD_MISMATCH_IDX + sizeof "archive/member file descriptor mismatch")
  N_("offset out of range")
  "\0"
#define ELF_E_NOT_NUL_SECTION_IDX \
  (ELF_E_OFFSET_RANGE_IDX + sizeof "offset out of range")
  N_("cannot manipulate null section")
  "\0"
#define ELF_E_DATA_MISMATCH_IDX \
  (ELF_E_NOT_NUL_SECTION_IDX + sizeof "cannot manipulate null section")
  N_("data/scn mismatch")
  "\0"
#define ELF_E_INVALID_SECTION_HEADER_IDX \
  (ELF_E_DATA_MISMATCH_IDX + sizeof "data/scn mismatch")
  N_("invalid section header")
  "\0"
#define ELF_E_INVALID_DATA_IDX \
  (ELF_E_INVALID_SECTION_HEADER_IDX + sizeof "invalid section header")
  N_("invalid data")
  "\0"
#define ELF_E_DATA_ENCODING_IDX \
  (ELF_E_INVALID_DATA_IDX + sizeof "invalid data")
  N_("unknown data encoding")
  "\0"
#define ELF_E_SECTION_TOO_SMALL_IDX \
  (ELF_E_DATA_ENCODING_IDX + sizeof "unknown data encoding")
  N_("section `sh_size' too small for data")
  "\0"
#define ELF_E_INVALID_ALIGN_IDX \
  (ELF_E_SECTION_TOO_SMALL_IDX + sizeof "section `sh_size' too small for data")
  N_("invalid section alignment")
  "\0"
#define ELF_E_INVALID_SHENTSIZE_IDX \
  (ELF_E_INVALID_ALIGN_IDX + sizeof "invalid section alignment")
  N_("invalid section entry size")
  "\0"
#define ELF_E_UPDATE_RO_IDX \
  (ELF_E_INVALID_SHENTSIZE_IDX + sizeof "invalid section entry size")
  N_("update() for write on read-only file")
  "\0"
#define ELF_E_NOFILE_IDX \
  (ELF_E_UPDATE_RO_IDX + sizeof "update() for write on read-only file")
  N_("no such file")
  "\0"
#define ELF_E_GROUP_NOT_REL_IDX \
  (ELF_E_NOFILE_IDX + sizeof "no such file")
  N_("only relocatable files can contain section groups")
  "\0"
#define ELF_E_INVALID_PHDR_IDX \
  (ELF_E_GROUP_NOT_REL_IDX \
   + sizeof "only relocatable files can contain section groups")
  N_("program header only allowed in executables, shared objects, and \
core files")
  "\0"
#define ELF_E_NO_PHDR_IDX \
  (ELF_E_INVALID_PHDR_IDX \
   + sizeof "program header only allowed in executables, shared objects, and \
core files")
  N_("file has no program header")
  "\0"
#define ELF_E_INVALID_OFFSET_IDX \
  (ELF_E_NO_PHDR_IDX \
   + sizeof "file has no program header")
  N_("invalid offset")
  "\0"
#define ELF_E_INVALID_SECTION_TYPE_IDX \
  (ELF_E_INVALID_OFFSET_IDX \
   + sizeof "invalid offset")
  N_("invalid section type")
  "\0"
#define ELF_E_INVALID_SECTION_FLAGS_IDX \
  (ELF_E_INVALID_SECTION_TYPE_IDX \
   + sizeof "invalid section type")
  N_("invalid section flags")
  "\0"
#define ELF_E_NOT_COMPRESSED_IDX		\
  (ELF_E_INVALID_SECTION_FLAGS_IDX			\
   + sizeof "invalid section flags")
  N_("section does not contain compressed data")
  "\0"
#define ELF_E_ALREADY_COMPRESSED_IDX \
  (ELF_E_NOT_COMPRESSED_IDX \
   + sizeof "section does not contain compressed data")
  N_("section contains compressed data")
  "\0"
#define ELF_E_UNKNOWN_COMPRESSION_TYPE_IDX \
  (ELF_E_ALREADY_COMPRESSED_IDX \
   + sizeof "section contains compressed data")
  N_("unknown compression type")
  "\0"
#define ELF_E_COMPRESS_ERROR_IDX \
  (ELF_E_UNKNOWN_COMPRESSION_TYPE_IDX \
   + sizeof "unknown compression type")
  N_("cannot compress data")
  "\0"
#define ELF_E_DECOMPRESS_ERROR_IDX \
  (ELF_E_COMPRESS_ERROR_IDX \
   + sizeof "cannot compress data")
  N_("cannot decompress data")
};


static const uint_fast16_t msgidx[ELF_E_NUM] =
{
  [ELF_E_NOERROR] = ELF_E_NOERROR_IDX,
  [ELF_E_UNKNOWN_ERROR] = ELF_E_UNKNOWN_ERROR_IDX,
  [ELF_E_UNKNOWN_VERSION] = ELF_E_UNKNOWN_VERSION_IDX,
  [ELF_E_UNKNOWN_TYPE] = ELF_E_UNKNOWN_TYPE_IDX,
  [ELF_E_INVALID_HANDLE] = ELF_E_INVALID_HANDLE_IDX,
  [ELF_E_SOURCE_SIZE] = ELF_E_SOURCE_SIZE_IDX,
  [ELF_E_DEST_SIZE] = ELF_E_DEST_SIZE_IDX,
  [ELF_E_INVALID_ENCODING] = ELF_E_INVALID_ENCODING_IDX,
  [ELF_E_NOMEM] = ELF_E_NOMEM_IDX,
  [ELF_E_INVALID_FILE] = ELF_E_INVALID_FILE_IDX,
  [ELF_E_INVALID_ELF] = ELF_E_INVALID_ELF_IDX,
  [ELF_E_INVALID_OP] = ELF_E_INVALID_OP_IDX,
  [ELF_E_NO_VERSION] = ELF_E_NO_VERSION_IDX,
  [ELF_E_INVALID_CMD] = ELF_E_INVALID_CMD_IDX,
  [ELF_E_RANGE] = ELF_E_RANGE_IDX,
  [ELF_E_ARCHIVE_FMAG] = ELF_E_ARCHIVE_FMAG_IDX,
  [ELF_E_INVALID_ARCHIVE] = ELF_E_INVALID_ARCHIVE_IDX,
  [ELF_E_NO_ARCHIVE] = ELF_E_NO_ARCHIVE_IDX,
  [ELF_E_NO_INDEX] = ELF_E_NO_INDEX_IDX,
  [ELF_E_READ_ERROR] = ELF_E_READ_ERROR_IDX,
  [ELF_E_WRITE_ERROR] = ELF_E_WRITE_ERROR_IDX,
  [ELF_E_INVALID_CLASS] = ELF_E_INVALID_CLASS_IDX,
  [ELF_E_INVALID_INDEX] = ELF_E_INVALID_INDEX_IDX,
  [ELF_E_INVALID_OPERAND] = ELF_E_INVALID_OPERAND_IDX,
  [ELF_E_INVALID_SECTION] = ELF_E_INVALID_SECTION_IDX,
  [ELF_E_INVALID_COMMAND] = ELF_E_INVALID_COMMAND_IDX,
  [ELF_E_WRONG_ORDER_EHDR] = ELF_E_WRONG_ORDER_EHDR_IDX,
  [ELF_E_FD_DISABLED] = ELF_E_FD_DISABLED_IDX,
  [ELF_E_FD_MISMATCH] = ELF_E_FD_MISMATCH_IDX,
  [ELF_E_OFFSET_RANGE] = ELF_E_OFFSET_RANGE_IDX,
  [ELF_E_NOT_NUL_SECTION] = ELF_E_NOT_NUL_SECTION_IDX,
  [ELF_E_DATA_MISMATCH] = ELF_E_DATA_MISMATCH_IDX,
  [ELF_E_INVALID_SECTION_HEADER] = ELF_E_INVALID_SECTION_HEADER_IDX,
  [ELF_E_INVALID_DATA] = ELF_E_INVALID_DATA_IDX,
  [ELF_E_DATA_ENCODING] = ELF_E_DATA_ENCODING_IDX,
  [ELF_E_SECTION_TOO_SMALL] = ELF_E_SECTION_TOO_SMALL_IDX,
  [ELF_E_INVALID_ALIGN] = ELF_E_INVALID_ALIGN_IDX,
  [ELF_E_INVALID_SHENTSIZE] = ELF_E_INVALID_SHENTSIZE_IDX,
  [ELF_E_UPDATE_RO] = ELF_E_UPDATE_RO_IDX,
  [ELF_E_NOFILE] = ELF_E_NOFILE_IDX,
  [ELF_E_GROUP_NOT_REL] = ELF_E_GROUP_NOT_REL_IDX,
  [ELF_E_INVALID_PHDR] = ELF_E_INVALID_PHDR_IDX,
  [ELF_E_NO_PHDR] = ELF_E_NO_PHDR_IDX,
  [ELF_E_INVALID_OFFSET] = ELF_E_INVALID_OFFSET_IDX,
  [ELF_E_INVALID_SECTION_TYPE] = ELF_E_INVALID_SECTION_TYPE_IDX,
  [ELF_E_INVALID_SECTION_FLAGS] = ELF_E_INVALID_SECTION_FLAGS_IDX,
  [ELF_E_NOT_COMPRESSED] = ELF_E_NOT_COMPRESSED_IDX,
  [ELF_E_ALREADY_COMPRESSED] = ELF_E_ALREADY_COMPRESSED_IDX,
  [ELF_E_UNKNOWN_COMPRESSION_TYPE] = ELF_E_UNKNOWN_COMPRESSION_TYPE_IDX,
  [ELF_E_COMPRESS_ERROR] = ELF_E_COMPRESS_ERROR_IDX,
  [ELF_E_DECOMPRESS_ERROR] = ELF_E_DECOMPRESS_ERROR_IDX
};
#define nmsgidx ((int) (sizeof (msgidx) / sizeof (msgidx[0])))


void
internal_function
__libelf_seterrno (int value)
{
  global_error = value >= 0 && value < nmsgidx ? value : ELF_E_UNKNOWN_ERROR;
}


const char *
elf_errmsg (int error)
{
  int last_error = global_error;

  if (error == 0)
    {
      assert (msgidx[last_error] < sizeof (msgstr));
      return last_error != 0 ? _(msgstr + msgidx[last_error]) : NULL;
    }
  else if (error < -1 || error >= nmsgidx)
    return _(msgstr + ELF_E_UNKNOWN_ERROR_IDX);

  assert (msgidx[error == -1 ? last_error : error] < sizeof (msgstr));
  return _(msgstr + msgidx[error == -1 ? last_error : error]);
}
