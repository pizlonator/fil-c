/* Return section compression header.
   Copyright (C) 2015 Red Hat, Inc.
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

#include "libelfP.h"
#include <gelf.h>
#include <stddef.h>


GElf_Chdr *
gelf_getchdr (Elf_Scn *scn, GElf_Chdr *dest)
{
  if (scn == NULL)
    return NULL;

  if (dest == NULL)
    {
      __libelf_seterrno (ELF_E_INVALID_OPERAND);
      return NULL;
    }

  if (scn->elf->class == ELFCLASS32)
    {
      Elf32_Chdr *chdr = elf32_getchdr (scn);
      if (chdr == NULL)
	return NULL;
      dest->ch_type = chdr->ch_type;
      dest->ch_size = chdr->ch_size;
      dest->ch_addralign = chdr->ch_addralign;
    }
  else
    {
      Elf64_Chdr *chdr = elf64_getchdr (scn);
      if (chdr == NULL)
	return NULL;
      *dest = *chdr;
    }

  return dest;
}
INTDEF(gelf_getchdr)
