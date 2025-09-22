/* Select specific element in archive.
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


size_t
elf_rand (Elf *elf, size_t offset)
{
  /* Be gratious, the specs demand it.  */
  if (elf == NULL || elf->kind != ELF_K_AR)
    return 0;

  rwlock_wrlock (elf->lock);

  /* Save the old offset and set the offset.  */
  elf->state.ar.offset = elf->start_offset + offset;

  /* Get the next archive header.  */
  if (__libelf_next_arhdr_wrlock (elf) != 0)
    {
      /* Mark the archive header as unusable.  */
      elf->state.ar.elf_ar_hdr.ar_name = NULL;
      return 0;
    }

  rwlock_unlock (elf->lock);

  return offset;
}
