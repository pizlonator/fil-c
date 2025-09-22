/* Get symbol version information at the given index.
   Copyright (C) 1999, 2000, 2001, 2002, 2005, 2009, 2014, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1999.

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
#include <string.h>

#include "libelfP.h"


GElf_Versym *
gelf_getversym (Elf_Data *data, int ndx, GElf_Versym *dst)
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;
  Elf_Scn *scn;
  GElf_Versym *result;

  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_HALF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* This is the one place where we have to take advantage of the fact
     that an `Elf_Data' pointer is also a pointer to `Elf_Data_Scn'.
     The interface is broken so that it requires this hack.  */
  scn = data_scn->s;

  /* It's easy to handle this type.  It has the same size for 32 and
     64 bit objects.  */
  assert (sizeof (GElf_Versym) == sizeof (Elf32_Versym));
  assert (sizeof (GElf_Versym) == sizeof (Elf64_Versym));

  rwlock_rdlock (scn->elf->lock);

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  if (INVALID_NDX (ndx, GElf_Versym, data))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      result = NULL;
    }
  else
    {
      *dst = ((GElf_Versym *) data->d_buf)[ndx];

      result = dst;
    }

  rwlock_unlock (scn->elf->lock);

  return result;
}
