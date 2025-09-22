/* Retrieve file identification data.
   Copyright (C) 1998, 1999, 2000, 2002, 2004, 2015 Red Hat, Inc.
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

#include <stddef.h>

#include "libelfP.h"


char *
elf_getident (Elf *elf, size_t *ptr)
{
  /* In case this is no ELF file, the handle is invalid and we return
     NULL.  */
  if (elf == NULL || elf->kind != ELF_K_ELF)
    {
      if (ptr != NULL)
	*ptr = 0;
      return NULL;
    }

  /* We already read the ELF header.  Return a pointer to it and store
     the length in *PTR.  */
  if (ptr != NULL)
    *ptr = EI_NIDENT;

  return (char *) (elf->class == ELFCLASS32
		   || (offsetof (struct Elf, state.elf32.ehdr)
		       == offsetof (struct Elf, state.elf64.ehdr))
		   ? elf->state.elf32.ehdr->e_ident
		   : elf->state.elf64.ehdr->e_ident);
}
