/* Create new ELF header.
   Copyright (C) 1998, 1999, 2000, 2002, 2015 Red Hat, Inc.
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

#include <stdlib.h>
#include <string.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


ElfW2(LIBELFBITS,Ehdr) *
elfw2(LIBELFBITS,newehdr) (Elf *elf)
{
  ElfW2(LIBELFBITS,Ehdr) *result;

  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
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

  /* Don't create an ELF header if one already exists.  */
  if (elf->state.ELFW(elf,LIBELFBITS).ehdr == NULL)
    {
      /* We use the memory in the ELF descriptor.  */
      elf->state.ELFW(elf,LIBELFBITS).ehdr =
	&elf->state.ELFW(elf,LIBELFBITS).ehdr_mem;

      /* We clear this memory.  */
      memset (elf->state.ELFW(elf,LIBELFBITS).ehdr, '\0',
	      sizeof (ElfW2(LIBELFBITS,Ehdr)));

      /* Mark the ELF header has modified.  */
      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags |= ELF_F_DIRTY;
    }

  result = elf->state.ELFW(elf,LIBELFBITS).ehdr;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elfw2(LIBELFBITS,newehdr))
