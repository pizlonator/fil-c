/* Decompression support for libdwfl: zlib (gzip), bzlib (bzip2) or lzma (xz).
   Copyright (C) 2009, 2016 Red Hat, Inc.
   Copyright (C) 2022 Google LLC
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libelfP.h"
#include "libdwflP.h"

#if !USE_BZLIB
# define __libdw_bunzip2(...)	DWFL_E_BADELF
#endif

#if !USE_LZMA
# define __libdw_unlzma(...)	DWFL_E_BADELF
#endif

#if !USE_ZSTD
# define __libdw_unzstd(...)	DWFL_E_BADELF
#endif

/* Consumes and replaces *ELF only on success.  */
static Dwfl_Error
decompress (int fd __attribute__ ((unused)), Elf **elf)
{
  Dwfl_Error error = DWFL_E_BADELF;
  /* ELF cannot be decompressed, if there is no file descriptor.  */
  if (fd == -1)
    return error;
  void *buffer = NULL;
  size_t size = 0;

  const off_t offset = (*elf)->start_offset;
  void *const mapped = ((*elf)->map_address == NULL ? NULL
			: (*elf)->map_address + offset);
  const size_t mapped_size = (*elf)->maximum_size;
  if (mapped_size == 0)
    return error;

  error = __libdw_gunzip (fd, offset, mapped, mapped_size, &buffer, &size);
  if (error == DWFL_E_BADELF)
    error = __libdw_bunzip2 (fd, offset, mapped, mapped_size, &buffer, &size);
  if (error == DWFL_E_BADELF)
    error = __libdw_unlzma (fd, offset, mapped, mapped_size, &buffer, &size);
  if (error == DWFL_E_BADELF)
    error = __libdw_unzstd (fd, offset, mapped, mapped_size, &buffer, &size);

  if (error == DWFL_E_NOERROR)
    {
      if (unlikely (size == 0))
	{
	  error = DWFL_E_BADELF;
	  free (buffer);
	}
      else
	{
	  Elf *memelf = elf_memory (buffer, size);
	  if (memelf == NULL)
	    {
	      error = DWFL_E_LIBELF;
	      free (buffer);
	    }
	  else
	    {
	      memelf->flags |= ELF_F_MALLOCED;
	      elf_end (*elf);
	      *elf = memelf;
	    }
	}
    }
  else
    free (buffer);

  return error;
}

static Dwfl_Error
what_kind (int fd, Elf **elfp, Elf_Kind *kind, bool *may_close_fd)
{
  Dwfl_Error error = DWFL_E_NOERROR;
  *kind = elf_kind (*elfp);
  if (unlikely (*kind == ELF_K_NONE))
    {
      if (unlikely (*elfp == NULL))
	error = DWFL_E_LIBELF;
      else
	{
	  error = decompress (fd, elfp);
	  if (error == DWFL_E_NOERROR)
	    {
	      *may_close_fd = true;
	      *kind = elf_kind (*elfp);
	    }
	}
    }
  return error;
}

static Dwfl_Error
libdw_open_elf (int *fdp, Elf **elfp, bool close_on_fail, bool archive_ok,
		bool never_close_fd, bool bad_elf_ok, bool use_elfp)
{
  bool may_close_fd = false;

  Elf *elf =
      use_elfp ? *elfp : elf_begin (*fdp, ELF_C_READ_MMAP_PRIVATE, NULL);

  Elf_Kind kind;
  Dwfl_Error error = what_kind (*fdp, &elf, &kind, &may_close_fd);
  if (error == DWFL_E_BADELF)
    {
      /* It's not an ELF file or a compressed file.
	 See if it's an image with a header preceding the real file.  */

      off_t offset = elf->start_offset;
      error = __libdw_image_header (*fdp, &offset,
				    (elf->map_address == NULL ? NULL
				     : elf->map_address + offset),
				    elf->maximum_size);
      if (error == DWFL_E_NOERROR)
	{
	  /* Pure evil.  libelf needs some better interfaces.  */
	  elf->kind = ELF_K_AR;
	  elf->state.ar.elf_ar_hdr.ar_name = "libdwfl is faking you out";
	  elf->state.ar.elf_ar_hdr.ar_size = elf->maximum_size - offset;
	  elf->state.ar.offset = offset - sizeof (struct ar_hdr);
	  Elf *subelf = elf_begin (-1, ELF_C_READ_MMAP_PRIVATE, elf);
	  elf->kind = ELF_K_NONE;
	  if (unlikely (subelf == NULL))
	    error = DWFL_E_LIBELF;
	  else
	    {
	      subelf->parent = NULL;
	      subelf->flags |= elf->flags & (ELF_F_MMAPPED | ELF_F_MALLOCED);
	      elf->flags &= ~(ELF_F_MMAPPED | ELF_F_MALLOCED);
	      elf_end (elf);
	      elf = subelf;
	      error = what_kind (*fdp, &elf, &kind, &may_close_fd);
	    }
	}
    }

  if (error == DWFL_E_NOERROR
      && kind != ELF_K_ELF
      && !(archive_ok && kind == ELF_K_AR))
    error = DWFL_E_BADELF;

  /* This basically means, we keep a ELF_K_NONE Elf handle and return it.  */
  if (bad_elf_ok && error == DWFL_E_BADELF)
    error = DWFL_E_NOERROR;

  if (error != DWFL_E_NOERROR)
    {
      elf_end (elf);
      elf = NULL;
    }

  if (! never_close_fd
      && error == DWFL_E_NOERROR ? may_close_fd : close_on_fail)
    {
      close (*fdp);
      *fdp = -1;
    }

  *elfp = elf;
  return error;
}

Dwfl_Error internal_function
__libdw_open_file (int *fdp, Elf **elfp, bool close_on_fail, bool archive_ok)
{
  return libdw_open_elf (fdp, elfp, close_on_fail, archive_ok, false, false,
			 false);
}

Dwfl_Error internal_function
__libdw_open_elf_memory (char *data, size_t size, Elf **elfp, bool archive_ok)
{
  /* It is ok to use `fd == -1` here, because libelf uses it as a value for
     "no file opened" and code supports working with this value, and also
     `never_close_fd == false` is passed to prevent closing non-existent file.
     The only caveat is in `decompress` method, which doesn't support
     decompressing from memory, so reading compressed zImage using this method
     won't work.  */
  int fd = -1;
  *elfp = elf_memory (data, size);
  if (unlikely(*elfp == NULL))
    {
      return DWFL_E_LIBELF;
    }
  /* Allow using this ELF as reference for subsequent elf_begin calls.  */
  (*elfp)->cmd = ELF_C_READ_MMAP_PRIVATE;
  return libdw_open_elf (&fd, elfp, false, archive_ok, true, false, true);
}

Dwfl_Error internal_function
__libdw_open_elf (int fd, Elf **elfp)
{
  return libdw_open_elf (&fd, elfp, false, true, true, true, false);
}
