/* Retrieve uninterpreted file contents.
   Copyright (C) 1998, 1999, 2000, 2002, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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


char *
elf_rawfile (Elf *elf, size_t *ptr)
{
  char *result;

  if (elf == NULL)
    {
      /* No valid descriptor.  */
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
    error_out:
      if (ptr != NULL)
	*ptr = 0;
      return NULL;
    }

  /* If the file is not mmap'ed and not previously loaded, do it now.  */
  if (elf->map_address == NULL && __libelf_readall (elf) == NULL)
    goto error_out;

  rwlock_rdlock (elf->lock);
  if (ptr != NULL)
    *ptr = elf->maximum_size;

  result = (char *) elf->map_address + elf->start_offset;
  rwlock_unlock (elf->lock);

  return result;
}
