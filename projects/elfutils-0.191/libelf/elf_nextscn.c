/* Get next section.
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

#include <assert.h>
#include <libelf.h>
#include <stddef.h>

#include "libelfP.h"


Elf_Scn *
elf_nextscn (Elf *elf, Elf_Scn *scn)
{
  Elf_ScnList *list;
  Elf_Scn *result = NULL;

  if (elf == NULL)
    return NULL;

  rwlock_rdlock (elf->lock);

  if (scn == NULL)
    {
      /* If no section handle is given return the first (not 0th) section.
	 Set scn to the 0th section and perform nextscn.  */
      if (elf->class == ELFCLASS32
	   || (offsetof (Elf, state.elf32.scns)
	       == offsetof (Elf, state.elf64.scns)))
	list = &elf->state.elf32.scns;
      else
	list = &elf->state.elf64.scns;

      scn = &list->data[0];
    }
  else
    list = scn->list;

  if (scn + 1 < &list->data[list->cnt])
    result = scn + 1;
  else if (scn + 1 == &list->data[list->max]
	   && (list = list->next) != NULL)
    {
      /* If there is another element in the section list it must
         have at least one entry.  */
      assert (list->cnt > 0);
      result = &list->data[0];
    }

  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_nextscn)
