/* Update program header program header table entry.
   Copyright (C) 2000-2010 Red Hat, Inc.
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
gelf_update_phdr (Elf *elf, int ndx, GElf_Phdr *src)
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
      Elf32_Phdr *phdr = elf->state.elf32.phdr;

      /* We have to convert the data to the 32 bit format.  This might
	 overflow some fields so we have to test for this case before
	 copying.  */
      if (unlikely (src->p_offset > 0xffffffffull)
	  || unlikely (src->p_vaddr > 0xffffffffull)
	  || unlikely (src->p_paddr > 0xffffffffull)
	  || unlikely (src->p_filesz > 0xffffffffull)
	  || unlikely (src->p_memsz > 0xffffffffull)
	  || unlikely (src->p_align > 0xffffffffull))
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  goto out;
	}

      if (phdr == NULL)
	{
	  phdr = __elf32_getphdr_wrlock (elf);
	  if (phdr == NULL)
	    /* The error number is already set.  */
	    goto out;
	}

      /* Test whether the index is ok.  */
      size_t phnum;
      if (ndx >= elf->state.elf32.ehdr->e_phnum
	  && (elf->state.elf32.ehdr->e_phnum != PN_XNUM
	      || __elf_getphdrnum_rdlock (elf, &phnum) != 0
	      || (size_t) ndx >= phnum))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      /* Now correct the pointer to point to the correct element.  */
      phdr += ndx;

#define COPY(name) \
      phdr->name = src->name
      COPY (p_type);
      COPY (p_offset);
      COPY (p_vaddr);
      COPY (p_paddr);
      COPY (p_filesz);
      COPY (p_memsz);
      COPY (p_flags);
      COPY (p_align);
    }
  else
    {
      Elf64_Phdr *phdr = elf->state.elf64.phdr;

      if (phdr == NULL)
	{
	  phdr = __elf64_getphdr_wrlock (elf);
	  if (phdr == NULL)
	    /* The error number is already set.  */
	    goto out;
	}

      /* Test whether the index is ok.  */
      size_t phnum;
      if (ndx >= elf->state.elf64.ehdr->e_phnum
	  && (elf->state.elf64.ehdr->e_phnum != PN_XNUM
	      || __elf_getphdrnum_rdlock (elf, &phnum) != 0
	      || (size_t) ndx >= phnum))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      /* Just copy the data.  */
      memcpy (phdr + ndx, src, sizeof (Elf64_Phdr));
    }

  /* Mark the program header as modified.  */
  elf->state.elf.phdr_flags |= ELF_F_DIRTY;

  result = 1;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
