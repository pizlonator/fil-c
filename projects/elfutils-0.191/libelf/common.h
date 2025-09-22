/* Common definitions for handling files in memory or only on disk.
   Copyright (C) 1998, 1999, 2000, 2002, 2005, 2008 Red Hat, Inc.
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

#ifndef _COMMON_H
#define _COMMON_H       1

#include <stdlib.h>
#include <string.h>

#include "libelfP.h"

static inline Elf_Kind
__attribute__ ((unused))
determine_kind (void *buf, size_t len)
{
  /* First test for an archive.  */
  if (len >= SARMAG && memcmp (buf, ARMAG, SARMAG) == 0)
    return ELF_K_AR;

  /* Next try ELF files.  */
  if (len >= EI_NIDENT && memcmp (buf, ELFMAG, SELFMAG) == 0)
    {
      /* Could be an ELF file.  */
      int eclass = (int) ((unsigned char *) buf)[EI_CLASS];
      int data = (int) ((unsigned char *) buf)[EI_DATA];
      int version = (int) ((unsigned char *) buf)[EI_VERSION];

      if (eclass > ELFCLASSNONE && eclass < ELFCLASSNUM
	  && data > ELFDATANONE && data < ELFDATANUM
	  && version == EV_CURRENT)
	return ELF_K_ELF;
    }

  /* We do not know this file type.  */
  return ELF_K_NONE;
}


/* Allocate an Elf descriptor and fill in the generic information.  */
static inline Elf *
__attribute__ ((unused))
allocate_elf (int fildes, void *map_address, int64_t offset, size_t maxsize,
              Elf_Cmd cmd, Elf *parent, Elf_Kind kind, size_t extra)
{
  Elf *result = calloc (1, sizeof (Elf) + extra);
  if (result == NULL)
    __libelf_seterrno (ELF_E_NOMEM);
  else
    {
      result->kind = kind;
      result->ref_count = 1;
      result->cmd = cmd;
      result->fildes = fildes;
      result->start_offset = offset;
      result->maximum_size = maxsize;
      result->map_address = map_address;
      result->parent = parent;

      rwlock_init (result->lock);
    }

  return result;
}


/* Acquire lock for the descriptor and all children.  */
static void
__attribute__ ((unused))
libelf_acquire_all (Elf *elf)
{
  rwlock_wrlock (elf->lock);

  if (elf->kind == ELF_K_AR)
    {
      Elf *child = elf->state.ar.children;

      while (child != NULL)
	{
	  if (child->ref_count != 0)
	    libelf_acquire_all (child);
	  child = child->next;
	}
    }
}

/* Release own lock and those of the children.  */
static void
__attribute__ ((unused))
libelf_release_all (Elf *elf)
{
  if (elf->kind == ELF_K_AR)
    {
      Elf *child = elf->state.ar.children;

      while (child != NULL)
	{
	  if (child->ref_count != 0)
	    libelf_release_all (child);
	  child = child->next;
	}
    }

  rwlock_unlock (elf->lock);
}


/* Macro to convert endianness in place.  It determines the function it
   has to use itself.  */
#define CONVERT(Var) \
  (Var) = (sizeof (Var) == 1						      \
	   ? (unsigned char) (Var)					      \
	   : (sizeof (Var) == 2						      \
	      ? bswap_16 (Var)						      \
	      : (sizeof (Var) == 4					      \
		 ? bswap_32 (Var)					      \
		 : bswap_64 (Var))))

#define CONVERT_TO(Dst, Var) \
  (Dst) = (sizeof (Var) == 1						      \
	   ? (unsigned char) (Var)					      \
	   : (sizeof (Var) == 2						      \
	      ? bswap_16 (Var)						      \
	      : (sizeof (Var) == 4					      \
		 ? bswap_32 (Var)					      \
		 : bswap_64 (Var))))


#if BYTE_ORDER == LITTLE_ENDIAN
# define MY_ELFDATA	ELFDATA2LSB
#else
# define MY_ELFDATA	ELFDATA2MSB
#endif

#endif	/* common.h */
