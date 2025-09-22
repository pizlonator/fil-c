/* Return program header table entry.
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

#include <gelf.h>
#include <string.h>
#include <stdbool.h>

#include "libelfP.h"


GElf_Phdr *
gelf_getphdr (Elf *elf, int ndx, GElf_Phdr *dst)
{
  GElf_Phdr *result = NULL;

  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  if (dst == NULL)
    {
      __libelf_seterrno (ELF_E_INVALID_OPERAND);
      return NULL;
    }

  rwlock_rdlock (elf->lock);

  if (elf->class == ELFCLASS32)
    {
      /* Copy the elements one-by-one.  */
      Elf32_Phdr *phdr = elf->state.elf32.phdr;

      if (phdr == NULL)
	{
	  rwlock_unlock (elf->lock);
	  phdr = INTUSE(elf32_getphdr) (elf);
	  if (phdr == NULL)
	    /* The error number is already set.  */
	    return NULL;
	  rwlock_rdlock (elf->lock);
	}

      /* Test whether the index is ok.  */
      size_t phnum;
      if (__elf_getphdrnum_chk_rdlock (elf, &phnum) != 0
	  || (size_t) ndx >= phnum)
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      /* We know the result now.  */
      result = dst;

      /* Now correct the pointer to point to the correct element.  */
      phdr += ndx;

#define COPY(Name) result->Name = phdr->Name
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
      /* Copy the elements one-by-one.  */
      Elf64_Phdr *phdr = elf->state.elf64.phdr;

      if (phdr == NULL)
	{
	  rwlock_unlock (elf->lock);
	  phdr = INTUSE(elf64_getphdr) (elf);
	  if (phdr == NULL)
	    /* The error number is already set.  */
	    return NULL;
	  rwlock_rdlock (elf->lock);
	}

      /* Test whether the index is ok.  */
      size_t phnum;
      if (__elf_getphdrnum_chk_rdlock (elf, &phnum) != 0
	  || (size_t) ndx >= phnum)
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      /* We only have to copy the data.  */
      result = memcpy (dst, phdr + ndx, sizeof (GElf_Phdr));
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
