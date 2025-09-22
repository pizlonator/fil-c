/* Read header of next archive member.
   Copyright (C) 1998, 1999, 2000, 2002, 2008, 2015 Red Hat, Inc.
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

#include <assert.h>
#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"


Elf_Arhdr *
elf_getarhdr (Elf *elf)
{
  if (elf == NULL)
    return NULL;

  Elf *parent = elf->parent;

  /* Calling this function is not ok for any file type but archives.  */
  if (parent == NULL)
    {
      __libelf_seterrno (ELF_E_INVALID_OP);
      return NULL;
    }

  /* Make sure we have read the archive header.  */
  if (parent->state.ar.elf_ar_hdr.ar_name == NULL
      && __libelf_next_arhdr_wrlock (parent) != 0)
    {
      rwlock_wrlock (parent->lock);
      int st = __libelf_next_arhdr_wrlock (parent);
      rwlock_unlock (parent->lock);

      if (st != 0)
	/* Something went wrong.  Maybe there is no member left.  */
	return NULL;
    }

  /* We can be sure the parent is an archive.  */
  assert (parent->kind == ELF_K_AR);

  return &parent->state.ar.elf_ar_hdr;
}
