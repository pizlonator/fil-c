/* Get ELF header.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2005, 2015 Red Hat, Inc.
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

#include <gelf.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "libelfP.h"


GElf_Ehdr *
internal_function
__gelf_getehdr_rdlock (Elf *elf, GElf_Ehdr *dest)
{
  GElf_Ehdr *result = NULL;

  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* The following is an optimization: the ehdr element is at the same
     position in both the elf32 and elf64 structure.  */
  if (offsetof (struct Elf, state.elf32.ehdr)
      != offsetof (struct Elf, state.elf64.ehdr))
    abort ();
  /* Just pick one of the values.  */
 if (unlikely (elf->state.elf64.ehdr == NULL))
    /* Maybe no ELF header was created yet.  */
    __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
  else if (elf->class == ELFCLASS32)
    {
      Elf32_Ehdr *ehdr = elf->state.elf32.ehdr;

      /* Convert the 32-bit struct to an 64-bit one.  */
      memcpy (dest->e_ident, ehdr->e_ident, EI_NIDENT);
#define COPY(name) \
      dest->name = ehdr->name
      COPY (e_type);
      COPY (e_machine);
      COPY (e_version);
      COPY (e_entry);
      COPY (e_phoff);
      COPY (e_shoff);
      COPY (e_flags);
      COPY (e_ehsize);
      COPY (e_phentsize);
      COPY (e_phnum);
      COPY (e_shentsize);
      COPY (e_shnum);
      COPY (e_shstrndx);

      result = dest;
    }
  else
    result = memcpy (dest, elf->state.elf64.ehdr, sizeof (*dest));

  return result;
}

GElf_Ehdr *
gelf_getehdr (Elf *elf, GElf_Ehdr *dest)
{
  GElf_Ehdr *result;
  if (elf == NULL)
    return NULL;

  rwlock_rdlock (elf->lock);
  result = __gelf_getehdr_rdlock (elf, dest);
  rwlock_unlock (elf->lock);

  return result;
}
