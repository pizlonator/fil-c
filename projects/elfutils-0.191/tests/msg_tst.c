/* Copyright (C) 2002, 2005, 2006 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libelfP.h>

static struct
{
  int id;
  const char *expected;
} libelf_msgs[ELF_E_NUM] =
  {
    { ELF_E_NOERROR, "no error" },
    { ELF_E_UNKNOWN_ERROR, "unknown error" },
    { ELF_E_UNKNOWN_VERSION, "unknown version" },
    { ELF_E_UNKNOWN_TYPE, "unknown type" },
    { ELF_E_INVALID_HANDLE, "invalid `Elf' handle" },
    { ELF_E_SOURCE_SIZE, "invalid size of source operand" },
    { ELF_E_DEST_SIZE, "invalid size of destination operand" },
    { ELF_E_INVALID_ENCODING, "invalid encoding" },
    { ELF_E_NOMEM, "out of memory" },
    { ELF_E_INVALID_FILE, "invalid file descriptor" },
    { ELF_E_INVALID_ELF, "invalid ELF file data" },
    { ELF_E_INVALID_OP, "invalid operation" },
    { ELF_E_NO_VERSION, "ELF version not set" },
    { ELF_E_INVALID_CMD, "invalid command" },
    { ELF_E_RANGE, "offset out of range" },
    { ELF_E_ARCHIVE_FMAG, "invalid fmag field in archive header" },
    { ELF_E_INVALID_ARCHIVE, "invalid archive file" },
    { ELF_E_NO_ARCHIVE, "descriptor is not for an archive" },
    { ELF_E_NO_INDEX, "no index available" },
    { ELF_E_READ_ERROR, "cannot read data from file" },
    { ELF_E_WRITE_ERROR, "cannot write data to file" },
    { ELF_E_INVALID_CLASS, "invalid binary class" },
    { ELF_E_INVALID_INDEX, "invalid section index" },
    { ELF_E_INVALID_OPERAND, "invalid operand" },
    { ELF_E_INVALID_SECTION, "invalid section" },
    { ELF_E_INVALID_COMMAND, "invalid command" },
    { ELF_E_WRONG_ORDER_EHDR, "executable header not created first" },
    { ELF_E_FD_DISABLED, "file descriptor disabled" },
    { ELF_E_FD_MISMATCH, "archive/member file descriptor mismatch" },
    { ELF_E_OFFSET_RANGE, "offset out of range" },
    { ELF_E_NOT_NUL_SECTION, "cannot manipulate null section" },
    { ELF_E_DATA_MISMATCH, "data/scn mismatch" },
    { ELF_E_INVALID_SECTION_HEADER, "invalid section header" },
    { ELF_E_INVALID_DATA, "invalid data" },
    { ELF_E_DATA_ENCODING, "unknown data encoding" },
    { ELF_E_SECTION_TOO_SMALL, "section `sh_size' too small for data" },
    { ELF_E_INVALID_ALIGN, "invalid section alignment" },
    { ELF_E_INVALID_SHENTSIZE, "invalid section entry size" },
    { ELF_E_UPDATE_RO, "update() for write on read-only file" },
    { ELF_E_NOFILE, "no such file" },
    { ELF_E_GROUP_NOT_REL,
      "only relocatable files can contain section groups" },
    { ELF_E_INVALID_PHDR,
      "program header only allowed in executables, shared objects, \
and core files" },
    { ELF_E_NO_PHDR, "file has no program header" },
    { ELF_E_INVALID_OFFSET, "invalid offset" },
    { ELF_E_INVALID_SECTION_TYPE , "invalid section type" },
    { ELF_E_INVALID_SECTION_FLAGS , "invalid section flags" },
    { ELF_E_NOT_COMPRESSED, "section does not contain compressed data" },
    { ELF_E_ALREADY_COMPRESSED, "section contains compressed data" },
    { ELF_E_UNKNOWN_COMPRESSION_TYPE, "unknown compression type" },
    { ELF_E_COMPRESS_ERROR, "cannot compress data" },
    { ELF_E_DECOMPRESS_ERROR, "cannot decompress data" }
  };


int
main (void)
{
  size_t cnt;
  int result = EXIT_SUCCESS;

  /* Clear the error state.  */
  (void) elf_errno ();

  /* Check all the messages of libelf.  */
  for (cnt = 1; cnt < ELF_E_NUM; ++cnt)
    {
      const char *str = elf_errmsg (libelf_msgs[cnt].id);

      if (strcmp (str, libelf_msgs[cnt].expected) != 0)
	{
	  printf ("libelf msg %zu: expected \"%s\", got \"%s\"\n",
		  cnt, libelf_msgs[cnt].expected, str);
	  result = EXIT_FAILURE;
	}
    }

  return result;
}
