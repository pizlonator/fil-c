/* Get additional symbol information from symbol table at the given index.
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
#include <string.h>

#include "libelfP.h"


GElf_Syminfo *
gelf_getsyminfo (Elf_Data *data, int ndx, GElf_Syminfo *dst)
{
  GElf_Syminfo *result = NULL;

  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_SYMINFO))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* The types for 32 and 64 bit are the same.  Lucky us.  */
  assert (sizeof (GElf_Syminfo) == sizeof (Elf32_Syminfo));
  assert (sizeof (GElf_Syminfo) == sizeof (Elf64_Syminfo));

  rwlock_rdlock (((Elf_Data_Scn *) data)->s->elf->lock);

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  if (INVALID_NDX (ndx, GElf_Syminfo, data))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      goto out;
    }

  *dst = ((GElf_Syminfo *) data->d_buf)[ndx];

  result = dst;

 out:
  rwlock_unlock (((Elf_Data_Scn *) data)->s->elf->lock);

  return result;
}
