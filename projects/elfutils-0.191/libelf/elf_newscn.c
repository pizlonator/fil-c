/* Append new section.
   Copyright (C) 1998,1999,2000,2001,2002,2005,2009,2014,2015 Red Hat, Inc.
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
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "libelfP.h"


Elf_Scn *
elf_newscn (Elf *elf)
{
  Elf_Scn *result = NULL;
  bool first = false;

  if (elf == NULL)
    return NULL;

  /* We rely on the prefix of the `elf', `elf32', and `elf64' element
     being the same.  */
  assert (offsetof (Elf, state.elf.scns_last)
	  == offsetof (Elf, state.elf32.scns_last));
  assert (offsetof (Elf, state.elf.scns_last)
	  == offsetof (Elf, state.elf64.scns_last));
  assert (offsetof (Elf, state.elf32.scns)
	  == offsetof (Elf, state.elf64.scns));

  rwlock_wrlock (elf->lock);

 again:
  if (elf->state.elf.scns_last->cnt < elf->state.elf.scns_last->max)
    {
      result = &elf->state.elf.scns_last->data[elf->state.elf.scns_last->cnt];

      if (++elf->state.elf.scns_last->cnt == 1
	  && (elf->state.elf.scns_last
	      == (elf->class == ELFCLASS32
		  || (offsetof (Elf, state.elf32.scns)
		      == offsetof (Elf, state.elf64.scns))
		  ? &elf->state.elf32.scns : &elf->state.elf64.scns)))
	/* This is zeroth section.  */
	first = true;
      else
	{
	  assert (elf->state.elf.scns_last->cnt > 1);
	  result->index = result[-1].index + 1;
	}
    }
  else
    {
      /* We must allocate a new element.  */
      Elf_ScnList *newp = NULL;

      assert (elf->state.elf.scnincr > 0);

      if (
#if SIZE_MAX <= 4294967295U
	  likely (elf->state.elf.scnincr
		  < SIZE_MAX / 2 / sizeof (Elf_Scn) - sizeof (Elf_ScnList))
#else
	  1
#endif
	  )
      newp = calloc (1, sizeof (Elf_ScnList)
			+ ((elf->state.elf.scnincr *= 2)
			   * sizeof (Elf_Scn)));
      if (newp == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}

      result = &newp->data[0];

      /* One section used.  */
      ++newp->cnt;

      /* This is the number of sections we allocated.  */
      newp->max = elf->state.elf.scnincr;

      /* Remember the index for the first section in this block.  */
      newp->data[0].index
	= 1 + elf->state.elf.scns_last->data[elf->state.elf.scns_last->max - 1].index;

      /* Enqueue the new list element.  */
      elf->state.elf.scns_last = elf->state.elf.scns_last->next = newp;
    }

  /* Create a section header for this section.  */
  if (elf->class == ELFCLASS32)
    {
      result->shdr.e32 = calloc (1, sizeof (Elf32_Shdr));
      if (result->shdr.e32 == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}
    }
  else
    {
      result->shdr.e64 = calloc (1, sizeof (Elf64_Shdr));
      if (result->shdr.e64 == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}
    }

  result->elf = elf;
  result->shdr_flags = ELF_F_DIRTY | ELF_F_MALLOCED;
  result->list = elf->state.elf.scns_last;

  /* Initialize the data part.  */
  result->data_read = 1;
  if (unlikely (first))
    {
      /* For the first section we mark the data as already available.  */
      //result->data_list_rear = &result->data_list;
      first = false;
      goto again;
    }

  result->flags |= ELF_F_DIRTY;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
