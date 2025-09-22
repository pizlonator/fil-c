/* Get the section index of the extended section index table.
   Copyright (C) 2007 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2007.

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
elf_scnshndx (Elf_Scn *scn)
{
  if (unlikely (scn->shndx_index == 0))
    {
      /* We do not have the value yet.  We get it as a side effect of
	 getting a section header.  */
      GElf_Shdr shdr_mem;
      (void) INTUSE(gelf_getshdr) (scn, &shdr_mem);
    }

  return scn->shndx_index;
}
INTDEF(elf_scnshndx)
