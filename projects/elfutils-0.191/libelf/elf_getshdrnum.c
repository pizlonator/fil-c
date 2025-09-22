/* Return number of sections in the ELF file.
   Copyright (C) 2002, 2009, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include <gelf.h>
#include <stddef.h>

#include "libelfP.h"


int
internal_function
__elf_getshdrnum_rdlock (Elf *elf, size_t *dst)
{
  int result = 0;
  int idx;

  if (elf == NULL)
    return -1;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  idx = elf->state.elf.scns_last->cnt;
  if (idx != 0
      || (elf->state.elf.scns_last
	  != (elf->class == ELFCLASS32
	      || (offsetof (Elf, state.elf32.scns)
		  == offsetof (Elf, state.elf64.scns))
	      ? &elf->state.elf32.scns : &elf->state.elf64.scns)))
    /* There is at least one section.  */
    *dst = 1 + elf->state.elf.scns_last->data[idx - 1].index;
  else
    *dst = 0;

  return result;
}

int
elf_getshdrnum (Elf *elf, size_t *dst)
{
  int result;

  if (elf == NULL)
    return -1;

  rwlock_rdlock (elf->lock);
  result = __elf_getshdrnum_rdlock (elf, dst);
  rwlock_unlock (elf->lock);

  return result;
}
/* Alias for the deprecated name.  */
strong_alias (elf_getshdrnum, elf_getshnum)
