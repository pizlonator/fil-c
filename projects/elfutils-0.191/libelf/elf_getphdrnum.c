/* Return number of program headers in the ELF file.
   Copyright (C) 2010, 2014, 2015, 2016 Red Hat, Inc.
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
#include <stddef.h>

#include "libelfP.h"


int
internal_function
__elf_getphdrnum_rdlock (Elf *elf, size_t *dst)
{
 if (unlikely (elf->state.elf64.ehdr == NULL))
   {
     /* Maybe no ELF header was created yet.  */
     *dst = 0;
     __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
     return -1;
   }

 *dst = (elf->class == ELFCLASS32
	 ? elf->state.elf32.ehdr->e_phnum
	 : elf->state.elf64.ehdr->e_phnum);

 if (*dst == PN_XNUM)
   {
     const Elf_ScnList *const scns = (elf->class == ELFCLASS32
				      ? &elf->state.elf32.scns
				      : &elf->state.elf64.scns);

     /* If there are no section headers, perhaps this is really just 65536
	written without PN_XNUM support.  Either that or it's bad data.  */

     if (elf->class == ELFCLASS32)
       {
	 if (likely (scns->cnt > 0))
	   {
	     Elf_Scn *scn = &elf->state.elf32.scns.data[0];
	     Elf32_Shdr *shdr = scn->shdr.e32 ?: __elf32_getshdr_rdlock (scn);
	     if (shdr)
	       *dst = shdr->sh_info;
	   }
       }
     else
       {
	 if (likely (scns->cnt > 0))
	   {
	     Elf_Scn *scn = &elf->state.elf64.scns.data[0];
	     Elf64_Shdr *shdr = scn->shdr.e64 ?: __elf64_getshdr_rdlock (scn);
	     if (shdr)
	       *dst = shdr->sh_info;
	   }
       }
   }

 return 0;
}

int
internal_function
__elf_getphdrnum_chk_rdlock (Elf *elf, size_t *dst)
{
  int result = __elf_getphdrnum_rdlock (elf, dst);

  /* If the phdrs haven't been created or read in yet then do some
     sanity checking to make sure phnum and phoff are consistent.  */
  if (elf->state.elf.phdr == NULL)
    {
      Elf64_Off off = (elf->class == ELFCLASS32
		       ? elf->state.elf32.ehdr->e_phoff
		       : elf->state.elf64.ehdr->e_phoff);
      if (unlikely (off == 0))
	{
	  *dst = 0;
	  return result;
	}

      if (unlikely (off >= elf->maximum_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  return -1;
	}

      /* Check for too many sections.  */
      size_t phdr_size = (elf->class == ELFCLASS32
			  ? sizeof (Elf32_Phdr) : sizeof (Elf64_Phdr));
      if (unlikely (*dst > SIZE_MAX / phdr_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  return -1;
	}

      /* Truncated file?  Don't return more than can be indexed.  */
      if (unlikely (elf->maximum_size - off < *dst * phdr_size))
	*dst = (elf->maximum_size - off) / phdr_size;
    }

  return result;
}

int
elf_getphdrnum (Elf *elf, size_t *dst)
{
  int result;

  if (elf == NULL)
    return -1;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  rwlock_rdlock (elf->lock);
  result = __elf_getphdrnum_chk_rdlock (elf, dst);
  rwlock_unlock (elf->lock);

  return result;
}
