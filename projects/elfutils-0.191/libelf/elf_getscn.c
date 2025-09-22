/* Get section at specific index.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2004, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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
#include <stddef.h>
#include <stdlib.h>

#include "libelfP.h"


Elf_Scn *
elf_getscn (Elf *elf, size_t idx)
{
  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  rwlock_rdlock (elf->lock);

  Elf_Scn *result = NULL;

  /* Find the section in the list.  */
  Elf_ScnList *runp = (elf->class == ELFCLASS32
		       || (offsetof (struct Elf, state.elf32.scns)
			   == offsetof (struct Elf, state.elf64.scns))
		       ? &elf->state.elf32.scns : &elf->state.elf64.scns);

  /* Section zero is special.  It always exists even if there is no
     "first" section.  And it is needed to store "overflow" values
     from the Elf header.  */
  if (idx == 0 && runp->cnt == 0 && runp->max > 0)
    {
      Elf_Scn *scn0 = &runp->data[0];
      if (elf->class == ELFCLASS32)
	{
	  scn0->shdr.e32 = calloc (1, sizeof (Elf32_Shdr));
	  if (scn0->shdr.e32 == NULL)
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      goto out;
	    }
	}
      else
	{
	  scn0->shdr.e64 = calloc (1, sizeof (Elf64_Shdr));
	  if (scn0->shdr.e64 == NULL)
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      goto out;
	    }
	}
      scn0->elf = elf;
      scn0->shdr_flags = ELF_F_DIRTY | ELF_F_MALLOCED;
      scn0->list = elf->state.elf.scns_last;
      scn0->data_read = 1;
      runp->cnt = 1;
    }

  while (1)
    {
      if (idx < runp->max)
	{
	  if (idx < runp->cnt)
	    result = &runp->data[idx];
	  else
	    __libelf_seterrno (ELF_E_INVALID_INDEX);
	  break;
	}

      idx -= runp->max;

      runp = runp->next;
      if (runp == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  break;
	}
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_getscn)
