/* Create new ELF header.
   Copyright (C) 2005, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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
#include <stdlib.h>

#include "libelfP.h"


Elf_Scn *
gelf_offscn (Elf *elf, GElf_Off offset)
{
  if (elf->class == ELFCLASS32)
    {
      if ((Elf32_Off) offset != offset)
	{
	  __libelf_seterrno (ELF_E_INVALID_OFFSET);
	  return NULL;
	}

      return INTUSE(elf32_offscn) (elf, (Elf32_Off) offset);
    }

  return INTUSE(elf64_offscn) (elf, offset);
}
