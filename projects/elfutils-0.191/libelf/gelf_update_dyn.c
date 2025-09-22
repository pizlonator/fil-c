/* Update information in dynamic table at the given index.
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

#include <gelf.h>
#include <string.h>

#include "libelfP.h"


int
gelf_update_dyn (Elf_Data *data, int ndx, GElf_Dyn *src)
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;
  Elf_Scn *scn;
  int result = 0;

  if (data == NULL)
    return 0;

  if (unlikely (data_scn->d.d_type != ELF_T_DYN))
    {
      /* The type of the data better should match.  */
      __libelf_seterrno (ELF_E_DATA_MISMATCH);
      return 0;
    }

  scn = data_scn->s;
  rwlock_wrlock (scn->elf->lock);

  if (scn->elf->class == ELFCLASS32)
    {
      Elf32_Dyn *dyn;

      /* There is the possibility that the values in the input are
	 too large.  */
      if (unlikely (src->d_tag < -0x80000000ll)
	  || unlikely (src->d_tag > 0x7fffffffll)
	  || unlikely (src->d_un.d_val > 0xffffffffull))
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  goto out;
	}

      /* Check whether we have to resize the data buffer.  */
      if (INVALID_NDX (ndx, Elf32_Dyn, &data_scn->d))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      dyn = &((Elf32_Dyn *) data_scn->d.d_buf)[ndx];

      dyn->d_tag = src->d_tag;
      dyn->d_un.d_val = src->d_un.d_val;
    }
  else
    {
      /* Check whether we have to resize the data buffer.  */
      if (INVALID_NDX (ndx, Elf64_Dyn, &data_scn->d))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      ((Elf64_Dyn *) data_scn->d.d_buf)[ndx] = *src;
    }

  result = 1;

  /* Mark the section as modified.  */
  scn->flags |= ELF_F_DIRTY;

 out:
  rwlock_unlock (scn->elf->lock);

  return result;
}
