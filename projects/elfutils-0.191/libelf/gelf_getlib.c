/* Get library from table at the given index.
   Copyright (C) 2004, 2005, 2009, 2014, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

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


GElf_Lib *
gelf_getlib (Elf_Data *data, int ndx, GElf_Lib *dst)
{
  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_LIB))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;

  rwlock_rdlock (data_scn->s->elf->lock);

  /* The on disk format of Elf32_Lib and Elf64_Lib is identical.  So
     we can simplify things significantly.  */
  assert (sizeof (GElf_Lib) == sizeof (Elf32_Lib));
  assert (sizeof (GElf_Lib) == sizeof (Elf64_Lib));

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  GElf_Lib *result = NULL;
  if (INVALID_NDX (ndx, GElf_Lib, data))
    __libelf_seterrno (ELF_E_INVALID_INDEX);
  else
    {
      *dst = ((GElf_Lib *) data->d_buf)[ndx];

      result = dst;
    }

  rwlock_unlock (data_scn->s->elf->lock);

  return result;
}
