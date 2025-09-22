/* Advance in archive to next element.
   Copyright (C) 1998-2009, 2015 Red Hat, Inc.
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

#include <assert.h>
#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"


Elf_Cmd
elf_next (Elf *elf)
{
  Elf *parent;
  Elf_Cmd ret;

  /* Be gratious, the specs demand it.  */
  if (elf == NULL || elf->parent == NULL)
    return ELF_C_NULL;

  /* We can be sure the parent is an archive.  */
  parent = elf->parent;
  assert (parent->kind == ELF_K_AR);

  rwlock_wrlock (parent->lock);

  /* Now advance the offset.  */
  parent->state.ar.offset += (sizeof (struct ar_hdr)
			      + ((parent->state.ar.elf_ar_hdr.ar_size + 1)
				 & ~1l));

  /* Get the next archive header.  */
  ret = __libelf_next_arhdr_wrlock (parent) != 0 ? ELF_C_NULL : elf->cmd;

  /* If necessary, mark the archive header as unusable.  */
  if (ret == ELF_C_NULL)
    parent->state.ar.elf_ar_hdr.ar_name = NULL;

  rwlock_unlock (parent->lock);

  return ret;
}
