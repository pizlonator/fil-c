/* Create new, empty section data.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2015 Red Hat, Inc.
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

#include <stddef.h>
#include <stdlib.h>

#include "libelfP.h"


Elf_Data *
elf_newdata (Elf_Scn *scn)
{
  Elf_Data_List *result = NULL;

  if (scn == NULL)
    return NULL;

  if (unlikely (scn->index == 0))
    {
      /* It is not allowed to add something to the 0th section.  */
      __libelf_seterrno (ELF_E_NOT_NUL_SECTION);
      return NULL;
    }

  if (scn->elf->class == ELFCLASS32
      || (offsetof (struct Elf, state.elf32.ehdr)
	  == offsetof (struct Elf, state.elf64.ehdr))
      ? scn->elf->state.elf32.ehdr == NULL
      : scn->elf->state.elf64.ehdr == NULL)
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      return NULL;
    }

  rwlock_wrlock (scn->elf->lock);

  /* data_read is set when data has been read from the ELF image or
     when a new section has been created by elf_newscn.  If data has
     been read from the ELF image, then rawdata_base will point to raw
     data.  If data_read has been set by elf_newscn, then rawdata_base
     will be NULL.  data_list_rear will be set by elf_getdata if the
     data has been converted, or by this function, elf_newdata, when
     new data has been added.

     Currently elf_getdata and elf_update rely on the fact that when
     data_list_read is not NULL all they have to do is walk the data
     list. They will ignore any (unread) raw data in that case.

     So we need to make sure the data list is setup if there is
     already data available.  */
  if (scn->data_read
      && scn->rawdata_base != NULL
      && scn->data_list_rear == NULL)
    __libelf_set_data_list_rdlock (scn, 1);

  if (scn->data_read && scn->data_list_rear == NULL)
    {
      /* This means the section was created by the user and this is the
	 first data.  */
      result = &scn->data_list;
      result->flags = ELF_F_DIRTY;
    }
  else
    {
      /* It would be more efficient to create new data without
	 reading/converting the data from the file.  But then we
	 have to remember this.  Currently elf_getdata and
	 elf_update rely on the fact that they don't have to
	 load/convert any data if data_list_rear is set.  */
      if (scn->data_read == 0)
	{
	  if (__libelf_set_rawdata_wrlock (scn) != 0)
	    /* Something went wrong.  The error value is already set.  */
	    goto out;
	  __libelf_set_data_list_rdlock (scn, 1);
	}

      /* Create a new, empty data descriptor.  */
      result = calloc (1, sizeof (Elf_Data_List));
      if (result == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}

      result->flags = ELF_F_DIRTY | ELF_F_MALLOCED;
    }

  /* Set the predefined values.  */
  result->data.d.d_version = EV_CURRENT;

  result->data.s = scn;

  /* Add to the end of the list.  */
  if (scn->data_list_rear != NULL)
    scn->data_list_rear->next = result;

  scn->data_list_rear = result;

 out:
  rwlock_unlock (scn->elf->lock);

  /* Please note that the following is thread safe and is also defined
     for RESULT == NULL since it still return NULL.  */
  return &result->data.d;
}
