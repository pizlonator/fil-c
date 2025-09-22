/* Get ELF header.
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

#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


static ElfW2(LIBELFBITS,Ehdr) *
getehdr_impl (Elf *elf, int wrlock)
{
  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

 again:
  if (elf->class == 0)
    {
      if (!wrlock)
	{
	  rwlock_unlock (elf->lock);
	  rwlock_wrlock (elf->lock);
	  wrlock = 1;
	  goto again;
	}
      elf->class = ELFW(ELFCLASS,LIBELFBITS);
    }
  else if (unlikely (elf->class != ELFW(ELFCLASS,LIBELFBITS)))
    {
      __libelf_seterrno (ELF_E_INVALID_CLASS);
      return NULL;
    }

  return elf->state.ELFW(elf,LIBELFBITS).ehdr;
}

ElfW2(LIBELFBITS,Ehdr) *
internal_function
__elfw2(LIBELFBITS,getehdr_wrlock) (Elf *elf)
{
  return getehdr_impl (elf, 1);
}

ElfW2(LIBELFBITS,Ehdr) *
elfw2(LIBELFBITS,getehdr) (Elf *elf)
{
  ElfW2(LIBELFBITS,Ehdr) *result;
  if (elf == NULL)
    return NULL;

  rwlock_rdlock (elf->lock);
  result = getehdr_impl (elf, 0);
  rwlock_unlock (elf->lock);

  return result;
}
