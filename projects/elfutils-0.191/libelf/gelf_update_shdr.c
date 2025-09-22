/* Update section header.
   Copyright (C) 2000, 2001, 2002, 2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <gelf.h>
#include <string.h>

#include "libelfP.h"


int
gelf_update_shdr (Elf_Scn *scn, GElf_Shdr *src)
{
  int result = 0;
  Elf *elf;

  if (scn == NULL || src == NULL)
    return 0;

  elf = scn->elf;
  rwlock_wrlock (elf->lock);

  if (elf->class == ELFCLASS32)
    {
      Elf32_Shdr *shdr
	= scn->shdr.e32 ?: __elf32_getshdr_wrlock (scn);

      if (shdr == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_OPERAND);
	  goto out;
	}

      if (unlikely (src->sh_flags > 0xffffffffull)
	  || unlikely (src->sh_addr > 0xffffffffull)
	  || unlikely (src->sh_offset > 0xffffffffull)
	  || unlikely (src->sh_size > 0xffffffffull)
	  || unlikely (src->sh_addralign > 0xffffffffull)
	  || unlikely (src->sh_entsize > 0xffffffffull))
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  goto out;
	}

#define COPY(name) \
      shdr->name = src->name
      COPY (sh_name);
      COPY (sh_type);
      COPY (sh_flags);
      COPY (sh_addr);
      COPY (sh_offset);
      COPY (sh_size);
      COPY (sh_link);
      COPY (sh_info);
      COPY (sh_addralign);
      COPY (sh_entsize);
    }
  else
    {
      Elf64_Shdr *shdr
	= scn->shdr.e64 ?: __elf64_getshdr_wrlock (scn);

      if (shdr == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_OPERAND);
	  goto out;
	}

      /* We only have to copy the data.  */
      (void) memcpy (shdr, src, sizeof (GElf_Shdr));
    }

  /* Mark the section header as modified.  */
  scn->shdr_flags |= ELF_F_DIRTY;

  result = 1;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
