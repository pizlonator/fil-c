/* Get move structure at the given index.
   Copyright (C) 2000, 2001, 2002, 2005, 2009, 2014, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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


GElf_Move *
gelf_getmove (Elf_Data *data, int ndx, GElf_Move *dst)
{
  GElf_Move *result = NULL;
  Elf *elf;

  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_MOVE))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* The types for 32 and 64 bit are the same.  Lucky us.  */
  assert (sizeof (GElf_Move) == sizeof (Elf32_Move));
  assert (sizeof (GElf_Move) == sizeof (Elf64_Move));

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  if (INVALID_NDX (ndx, GElf_Move, data))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      goto out;
    }

  elf = ((Elf_Data_Scn *) data)->s->elf;
  rwlock_rdlock (elf->lock);

  *dst = ((GElf_Move *) data->d_buf)[ndx];

  rwlock_unlock (elf->lock);

  result = dst;

 out:
  return result;
}
