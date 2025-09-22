/* Return converted data from raw chunk of ELF file.
   Copyright (C) 2007, 2014, 2015 Red Hat, Inc.
   Copyright (C) 2022, 2023 Mark J. Wielaard <mark@klomp.org>
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

#include <assert.h>
#include <errno.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>

#include "libelfP.h"
#include "common.h"

static int
chunk_compare (const void *a, const void *b)
{
  Elf_Data_Chunk *da = (Elf_Data_Chunk *)a;
  Elf_Data_Chunk *db = (Elf_Data_Chunk *)b;

  if (da->offset != db->offset)
    return da->offset - db->offset;

  if (da->data.d.d_size != db->data.d.d_size)
    return da->data.d.d_size - db->data.d.d_size;

  return da->data.d.d_type - db->data.d.d_type;
}

Elf_Data *
elf_getdata_rawchunk (Elf *elf, int64_t offset, size_t size, Elf_Type type)
{
  if (unlikely (elf == NULL))
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      /* No valid descriptor.  */
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  if (unlikely (offset < 0 || (uint64_t) offset > elf->maximum_size
		|| elf->maximum_size - (uint64_t) offset < size))

    {
      /* Invalid request.  */
      __libelf_seterrno (ELF_E_INVALID_OP);
      return NULL;
    }

  if (type >= ELF_T_NUM)
    {
      __libelf_seterrno (ELF_E_UNKNOWN_TYPE);
      return NULL;
    }

  /* Get the raw bytes from the file.  */
  void *rawchunk;
  int flags = 0;
  Elf_Data *result = NULL;

  rwlock_rdlock (elf->lock);

  /* Maybe we already got this chunk?  */
  Elf_Data_Chunk key;
  key.offset = offset;
  key.data.d.d_size = size;
  key.data.d.d_type = type;
  Elf_Data_Chunk **found = tsearch (&key, &elf->state.elf.rawchunks,
				    &chunk_compare);
  if (found == NULL)
    goto nomem;

  /* Existing entry.  */
  if (*found != &key && *found != NULL)
    {
      result = &(*found)->data.d;
      goto out;
    }

  /* New entry.  Note that *found will point to the newly inserted
     (dummy) key.  We'll replace it with a real rawchunk when that is
     setup.  Make sure to tdelete the dummy key if anything goes
     wrong.  */

  size_t align = __libelf_type_align (elf->class, type);
  if (elf->map_address != NULL)
    {
    /* If the file is mmap'ed we can use it directly, if aligned for type.  */
      char *rawdata = elf->map_address + elf->start_offset + offset;
      if (((uintptr_t) rawdata & (align - 1)) == 0)
	rawchunk = rawdata;
      else
	{
	  /* We allocate the memory and memcpy it to get aligned data. */
	  rawchunk = malloc (size);
	  if (rawchunk == NULL)
	    goto nomem;
	  memcpy (rawchunk, rawdata, size);
	  flags = ELF_F_MALLOCED;
	}
    }
  else
    {
      /* We allocate the memory and read the data from the file.  */
      rawchunk = malloc (size);
      if (rawchunk == NULL)
	{
	nomem:
	  tdelete (&key, &elf->state.elf.rawchunks, &chunk_compare);
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}

      /* Read the file content.  */
      if (unlikely ((size_t) pread_retry (elf->fildes, rawchunk, size,
					  elf->start_offset + offset)
		    != size))
	{
	  /* Something went wrong.  */
	  tdelete (&key, &elf->state.elf.rawchunks, &chunk_compare);
	  free (rawchunk);
	  __libelf_seterrno (ELF_E_READ_ERROR);
	  goto out;
	}

      flags = ELF_F_MALLOCED;
    }

  /* Copy and/or convert the data as needed for aligned native-order access.  */
  void *buffer;
  if (elf->state.elf32.ehdr->e_ident[EI_DATA] == MY_ELFDATA)
    {
      if (((uintptr_t) rawchunk & (align - 1)) == 0)
	/* No need to copy, we can use the raw data.  */
	buffer = rawchunk;
      else
	{
	  /* A malloc'd block is always sufficiently aligned.  */
	  assert (flags == 0);

	  buffer = malloc (size);
	  if (unlikely (buffer == NULL))
	    goto nomem;
	  flags = ELF_F_MALLOCED;

	  /* The copy will be appropriately aligned for direct access.  */
	  memcpy (buffer, rawchunk, size);

	  free (rawchunk);
	}
    }
  else
    {
      if (flags)
	buffer = rawchunk;
      else
	{
	  buffer = malloc (size);
	  if (unlikely (buffer == NULL))
	    goto nomem;
	  flags = ELF_F_MALLOCED;
	}

      /* Call the conversion function.  */
      (*__elf_xfctstom[elf->class - 1][type])(buffer, rawchunk, size, 0);

      if (!flags)
	free (rawchunk);
    }

  /* Allocate the dummy container to point at this buffer.  */
  Elf_Data_Chunk *chunk = calloc (1, sizeof *chunk);
  if (chunk == NULL)
    {
      if (flags)
	free (buffer);
      goto nomem;
    }

  chunk->dummy_scn.elf = elf;
  chunk->dummy_scn.flags = flags;
  chunk->data.s = &chunk->dummy_scn;
  chunk->data.d.d_buf = buffer;
  chunk->data.d.d_size = size;
  chunk->data.d.d_type = type;
  chunk->data.d.d_align = align;
  chunk->data.d.d_version = EV_CURRENT;
  chunk->offset = offset;

  rwlock_unlock (elf->lock);
  rwlock_wrlock (elf->lock);

  *found = chunk;
  result = &chunk->data.d;

 out:
  rwlock_unlock (elf->lock);
  return result;
}
