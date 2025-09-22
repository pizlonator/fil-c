/* Update symbol information and section index in symbol table at the
   given index.
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
#include <stdlib.h>
#include <string.h>

#include "libelfP.h"


int
gelf_update_symshndx (Elf_Data *symdata, Elf_Data *shndxdata, int ndx,
		      GElf_Sym *src, Elf32_Word srcshndx)
{
  Elf_Data_Scn *symdata_scn = (Elf_Data_Scn *) symdata;
  Elf_Data_Scn *shndxdata_scn = (Elf_Data_Scn *) shndxdata;
  Elf_Scn *scn;
  Elf32_Word *shndx = NULL;
  int result = 0;

  if (symdata == NULL)
    return 0;

  if (unlikely (symdata_scn->d.d_type != ELF_T_SYM))
    {
      /* The type of the data better should match.  */
      __libelf_seterrno (ELF_E_DATA_MISMATCH);
      return 0;
    }

  scn = symdata_scn->s;
  /* We simply have to believe the user that the two sections belong to
     the same ELF file.  */
  rwlock_wrlock (scn->elf->lock);

  /* The user is not required to pass a data descriptor for an extended
     section index table.  */
  if (shndxdata_scn != NULL)
    {
      if (unlikely ((ndx + 1) * sizeof (Elf32_Word) > shndxdata_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      shndx = &((Elf32_Word *) shndxdata_scn->d.d_buf)[ndx];
    }
  /* But if s/he does not the extended sectio index must be zero.  */
  else if (unlikely (srcshndx != 0))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      goto out;
    }

  if (scn->elf->class == ELFCLASS32)
    {
      Elf32_Sym *sym;

      /* There is the possibility that the values in the input are
	 too large.  */
      if (unlikely (src->st_value > 0xffffffffull)
	  || unlikely (src->st_size > 0xffffffffull))
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  goto out;
	}

      /* Check whether we have to resize the data buffer.  */
      if (INVALID_NDX (ndx, Elf32_Sym, &symdata_scn->d))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      sym = &((Elf32_Sym *) symdata_scn->d.d_buf)[ndx];

#define COPY(name) \
      sym->name = src->name
      COPY (st_name);
      COPY (st_value);
      COPY (st_size);
      /* Please note that we can simply copy the `st_info' element since
	 the definitions of ELFxx_ST_BIND and ELFxx_ST_TYPE are the same
	 for the 64 bit variant.  */
      COPY (st_info);
      COPY (st_other);
      COPY (st_shndx);
    }
  else
    {
      /* Check whether we have to resize the data buffer.  */
      if (INVALID_NDX (ndx, Elf64_Sym, &symdata_scn->d))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      ((Elf64_Sym *) symdata_scn->d.d_buf)[ndx] = *src;
    }

  /* Now we can store the section index.  */
  if (shndx != NULL)
    *shndx = srcshndx;

  result = 1;

  /* Mark the section as modified.  */
  scn->flags |= ELF_F_DIRTY;

 out:
  rwlock_unlock (scn->elf->lock);

  return result;
}
