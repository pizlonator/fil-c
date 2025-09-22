/* Create new ELF program header table.
   Copyright (C) 1999-2010, 2014, 2015 Red Hat, Inc.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


ElfW2(LIBELFBITS,Phdr) *
elfw2(LIBELFBITS,newphdr) (Elf *elf, size_t count)
{
  ElfW2(LIBELFBITS,Phdr) *result;

  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* This check is correct, it is for sh_info, which is either
     Elf32_Word or Elf64_Word, both being 32 bits.  But count is size_t
     so might not fit on 32bit ELF files.  */
  if (unlikely ((ElfW2(LIBELFBITS,Word)) count != count))
    {
      __libelf_seterrno (ELF_E_INVALID_OPERAND);
      return NULL;
    }

  rwlock_wrlock (elf->lock);

  if (elf->class == 0)
    elf->class = ELFW(ELFCLASS,LIBELFBITS);
  else if (unlikely (elf->class != ELFW(ELFCLASS,LIBELFBITS)))
    {
      __libelf_seterrno (ELF_E_INVALID_CLASS);
      result = NULL;
      goto out;
    }

  if (unlikely (elf->state.ELFW(elf,LIBELFBITS).ehdr == NULL))
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      result = NULL;
      goto out;
    }

  /* A COUNT of zero means remove existing table.  */
  if (count == 0)
    {
      /* Free the old program header.  */
      if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL)
	{
	  if (elf->state.ELFW(elf,LIBELFBITS).phdr_flags & ELF_F_MALLOCED)
	    free (elf->state.ELFW(elf,LIBELFBITS).phdr);

	  /* Set the pointer to NULL.  */
	  elf->state.ELFW(elf,LIBELFBITS).phdr = NULL;
	  /* Set the `e_phnum' member to the new value.  */
	  elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phnum = 0;
	  /* Also clear any old PN_XNUM extended value.  */
	  if (elf->state.ELFW(elf,LIBELFBITS).scns.cnt > 0)
	    elf->state.ELFW(elf,LIBELFBITS).scns.data[0]
	      .shdr.ELFW(e,LIBELFBITS)->sh_info = 0;
	  /* Also set the size.  */
	  elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phentsize =
	    sizeof (ElfW2(LIBELFBITS,Phdr));

	  elf->state.ELFW(elf,LIBELFBITS).phdr_flags |= ELF_F_DIRTY;
	  elf->flags |= ELF_F_DIRTY;
	  __libelf_seterrno (ELF_E_NOERROR);
	}

      result = NULL;
    }
  else if (elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phnum != count
	   || count == PN_XNUM
	   || elf->state.ELFW(elf,LIBELFBITS).phdr == NULL)
    {
      if (unlikely (count > SIZE_MAX / sizeof (ElfW2(LIBELFBITS,Phdr))))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  result = NULL;
	  goto out;
	}

      Elf_Scn *scn0 = &elf->state.ELFW(elf,LIBELFBITS).scns.data[0];
      if (unlikely (count >= PN_XNUM && scn0->shdr.ELFW(e,LIBELFBITS) == NULL))
	{
	  /* Something is wrong with section zero, but we need it to write
	     the extended phdr count.  */
	  __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
	  result = NULL;
	  goto out;
	}

      /* Allocate a new program header with the appropriate number of
	 elements.  */
      result = (ElfW2(LIBELFBITS,Phdr) *)
	realloc (elf->state.ELFW(elf,LIBELFBITS).phdr,
		 count * sizeof (ElfW2(LIBELFBITS,Phdr)));
      if (result == NULL)
	__libelf_seterrno (ELF_E_NOMEM);
      else
	{
	  /* Now set the result.  */
	  elf->state.ELFW(elf,LIBELFBITS).phdr = result;
	  if (count >= PN_XNUM)
	    {
	      /* We have to write COUNT into the zeroth section's sh_info.  */
	      if (elf->state.ELFW(elf,LIBELFBITS).scns.cnt == 0)
		{
		  assert (elf->state.ELFW(elf,LIBELFBITS).scns.max > 0);
		  elf->state.ELFW(elf,LIBELFBITS).scns.cnt = 1;
		}
	      scn0->shdr.ELFW(e,LIBELFBITS)->sh_info = count;
	      scn0->shdr_flags |= ELF_F_DIRTY;
	      elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phnum = PN_XNUM;
	    }
	  else
	    /* Set the `e_phnum' member to the new value.  */
	    elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phnum = count;
	  /* Clear the whole memory.  */
	  memset (result, '\0', count * sizeof (ElfW2(LIBELFBITS,Phdr)));
	  /* Also set the size.  */
	  elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phentsize =
	    elf_typesize (LIBELFBITS, ELF_T_PHDR, 1);
	  /* Remember we allocated the array and mark the structure is
	     modified.  */
	  elf->state.ELFW(elf,LIBELFBITS).phdr_flags |=
	    ELF_F_DIRTY | ELF_F_MALLOCED;
	  /* We have to rewrite the entire file if the size of the
	     program header is changed.  */
	  elf->flags |= ELF_F_DIRTY;
	}
    }
  else
    {
      /* We have the same number of entries.  Just clear the array.  */
      assert (elf->state.ELFW(elf,LIBELFBITS).ehdr->e_phentsize
	      == elf_typesize (LIBELFBITS, ELF_T_PHDR, 1));

      /* Mark the structure as modified.  */
      elf->state.ELFW(elf,LIBELFBITS).phdr_flags |= ELF_F_DIRTY;

      result = elf->state.ELFW(elf,LIBELFBITS).phdr;
      memset (result, '\0', count * sizeof (ElfW2(LIBELFBITS,Phdr)));
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elfw2(LIBELFBITS,newphdr))
