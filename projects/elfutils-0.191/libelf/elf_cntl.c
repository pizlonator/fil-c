/* Control an ELF file desrciptor.
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

#include "libelfP.h"


int
elf_cntl (Elf *elf, Elf_Cmd cmd)
{
  int result = 0;

  if (elf == NULL)
    return -1;

  if (elf->fildes == -1)
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  rwlock_wrlock (elf->lock);

  switch (cmd)
    {
    case ELF_C_FDREAD:
      /* If not all of the file is in the memory read it now.  */
      if (elf->map_address == NULL && __libelf_readall (elf) == NULL)
	{
	  /* We were not able to read everything.  */
	  result = -1;
	  break;
	}
      FALLTHROUGH;

    case ELF_C_FDDONE:
      /* Mark the file descriptor as not usable.  */
      elf->fildes = -1;
      break;

    default:
      __libelf_seterrno (ELF_E_INVALID_CMD);
      result = -1;
      break;
    }

  rwlock_unlock (elf->lock);

  return result;
}
