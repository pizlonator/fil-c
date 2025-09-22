/* Get note information at the supplied offset.
   Copyright (C) 2007, 2014, 2015, 2018 Red Hat, Inc.
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
#include <gelf.h>
#include <string.h>

#include "libelfP.h"

size_t
gelf_getnote (Elf_Data *data, size_t offset, GElf_Nhdr *result,
	      size_t *name_offset, size_t *desc_offset)
{
  if (data == NULL)
    return 0;

  if (unlikely (data->d_type != ELF_T_NHDR && data->d_type != ELF_T_NHDR8))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return 0;
    }

  /* It's easy to handle this type.  It has the same size for 32 and
     64 bit objects.  */
  assert (sizeof (GElf_Nhdr) == sizeof (Elf32_Nhdr));
  assert (sizeof (GElf_Nhdr) == sizeof (Elf64_Nhdr));

  rwlock_rdlock (((Elf_Data_Scn *) data)->s->elf->lock);

  /* The data is already in the correct form.  Just make sure the
     offset is OK.  */
  if (unlikely (offset > data->d_size
		|| data->d_size - offset < sizeof (GElf_Nhdr)))
    {
      __libelf_seterrno (ELF_E_OFFSET_RANGE);
      offset = 0;
    }
  else
    {
      const GElf_Nhdr *n = data->d_buf + offset;
      offset += sizeof *n;

      if (offset > data->d_size)
	offset = 0;
      else
	{
	  /* This is slightly tricky, offset is guaranteed to be 4
	     byte aligned, which is what we need for the name_offset.
	     And normally desc_offset is also 4 byte aligned, but not
	     for GNU Property notes, then it should be 8.  So align
	     the offset, after adding the namesz, and include padding
	     in descsz to get to the end.  */
	  *name_offset = offset;
	  if (n->n_namesz > data->d_size
	      || offset > data->d_size - n->n_namesz)
	    offset = 0;
	  else
	    {
	      offset += n->n_namesz;
	      /* Include padding.  Check below for overflow.  */
	      GElf_Word descsz = (data->d_type == ELF_T_NHDR8
				  ? NOTE_ALIGN8 (n->n_descsz)
				  : NOTE_ALIGN4 (n->n_descsz));

	      if (data->d_type == ELF_T_NHDR8)
		offset = NOTE_ALIGN8 (offset);
	      else
		offset = NOTE_ALIGN4 (offset);

	      if (unlikely (offset > data->d_size
			    || data->d_size - offset < descsz
			    || (descsz == 0 && n->n_descsz != 0)))
		offset = 0;
	      else
		{
		  *desc_offset = offset;
		  offset += descsz;
		  *result = *n;
		}
	    }
	}
    }

  rwlock_unlock (((Elf_Data_Scn *) data)->s->elf->lock);

  return offset;
}
