/* Get information from auxiliary vector at the given index.
   Copyright (C) 2007, 2015 Red Hat, Inc.
   This file is part of elfutils.

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


GElf_auxv_t *
gelf_getauxv (Elf_Data *data, int ndx, GElf_auxv_t *dst)
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;
  GElf_auxv_t *result = NULL;
  Elf *elf;

  if (data_scn == NULL)
    return NULL;

  if (unlikely (data_scn->d.d_type != ELF_T_AUXV))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  elf = data_scn->s->elf;

  rwlock_rdlock (elf->lock);

  /* This is the one place where we have to take advantage of the fact
     that an `Elf_Data' pointer is also a pointer to `Elf_Data_Scn'.
     The interface is broken so that it requires this hack.  */
  if (elf->class == ELFCLASS32)
    {
      Elf32_auxv_t *src;

      /* Here it gets a bit more complicated.  The format of the vector
	 entries has to be converted.  The user better have provided a
	 buffer where we can store the information.  While copying the data
	 we convert the format.  */
      if (unlikely ((ndx + 1) * sizeof (Elf32_auxv_t) > data_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      src = &((Elf32_auxv_t *) data_scn->d.d_buf)[ndx];

      /* This might look like a simple copy operation but it's
	 not.  There are zero- and sign-extensions going on.  */
      dst->a_type = src->a_type;
      dst->a_un.a_val = src->a_un.a_val;
    }
  else
    {
      /* If this is a 64 bit object it's easy.  */
      assert (sizeof (GElf_auxv_t) == sizeof (Elf64_auxv_t));

      /* The data is already in the correct form.  Just make sure the
	 index is OK.  */
      if (unlikely ((ndx + 1) * sizeof (GElf_auxv_t) > data_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      memcpy (dst, data_scn->d.d_buf + ndx * sizeof (GElf_auxv_t),
	      sizeof (GElf_auxv_t));
    }

  result = dst;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
