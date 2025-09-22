/* Update ELF header.
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
gelf_update_ehdr (Elf *elf, GElf_Ehdr *src)
{
  int result = 0;

  if (elf == NULL)
    return 0;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return 0;
    }

  rwlock_wrlock (elf->lock);

  if (elf->class == ELFCLASS32)
    {
      Elf32_Ehdr *ehdr = elf->state.elf32.ehdr;

      if (ehdr == NULL)
	{
	  __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
	  goto out;
	}

      /* We have to convert the data to the 32 bit format.  This might
	 overflow some fields so we have to test for this case before
	 copying.  */
      if (unlikely (src->e_entry > 0xffffffffull)
	  || unlikely (src->e_phoff > 0xffffffffull)
	  || unlikely (src->e_shoff > 0xffffffffull))
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  goto out;
	}

      /* Copy the data.  */
      memcpy (ehdr->e_ident, src->e_ident, EI_NIDENT);
#define COPY(name) \
      ehdr->name = src->name
      COPY (e_type);
      COPY (e_machine);
      COPY (e_version);
      COPY (e_entry);
      COPY (e_phoff);
      COPY (e_shoff);
      COPY (e_flags);
      COPY (e_ehsize);
      COPY (e_phentsize);
      COPY (e_phnum);
      COPY (e_shentsize);
      COPY (e_shnum);
      COPY (e_shstrndx);
    }
  else
    {
      Elf64_Ehdr *ehdr = elf->state.elf64.ehdr;

      if (ehdr == NULL)
	{
	  __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
	  goto out;
	}

      /* Just copy the data.  */
      memcpy (ehdr, src, sizeof (Elf64_Ehdr));
    }

  /* Mark the ELF header as modified.  */
  elf->state.elf.ehdr_flags |= ELF_F_DIRTY;

  result = 1;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
