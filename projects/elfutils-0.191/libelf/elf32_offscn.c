/* Get section at specific index.
   Copyright (C) 2005, 2008, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2005.

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

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


Elf_Scn *
elfw2(LIBELFBITS,offscn) (Elf *elf, ElfW2(LIBELFBITS,Off) offset)
{
  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  Elf_ScnList *runp = &elf->state.ELFW(elf,LIBELFBITS).scns;

  /* If we have not looked at section headers before,
     we might need to read them in first.  */
  if (runp->cnt > 0
      && unlikely (runp->data[0].shdr.ELFW(e,LIBELFBITS) == NULL)
      && unlikely (elfw2(LIBELFBITS,getshdr) (&runp->data[0]) == NULL))
    return NULL;

  rwlock_rdlock (elf->lock);

  Elf_Scn *result = NULL;

  /* Find the section in the list.  */
  while (1)
    {
      for (unsigned int i = 0; i < runp->cnt; ++i)
	if (runp->data[i].shdr.ELFW(e,LIBELFBITS)->sh_offset == offset)
	  {
	    result = &runp->data[i];

	    /* If this section is empty, the following one has the same
	       sh_offset.  We presume the caller is looking for a nonempty
	       section, so keep looking if this one is empty.  */
	    if (runp->data[i].shdr.ELFW(e,LIBELFBITS)->sh_size != 0
		&& runp->data[i].shdr.ELFW(e,LIBELFBITS)->sh_type != SHT_NOBITS)
	      goto out;
	  }

      runp = runp->next;
      if (runp == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_OFFSET);
	  break;
	}
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elfw2(LIBELFBITS,offscn))
