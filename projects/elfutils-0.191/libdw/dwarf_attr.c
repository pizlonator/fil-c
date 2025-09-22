/* Return specific DWARF attribute of a DIE.
   Copyright (C) 2003, 2005, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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

#include <dwarf.h>
#include "libdwP.h"


Dwarf_Attribute *
dwarf_attr (Dwarf_Die *die, unsigned int search_name, Dwarf_Attribute *result)
{
  if (die == NULL)
    return NULL;

  /* Search for the attribute with the given name.  */
  result->valp = __libdw_find_attr (die, search_name, &result->code,
				    &result->form);
  /* Always fill in the CU information.  */
  result->cu = die->cu;

  return result->valp != NULL && result->code == search_name ? result : NULL;
}
INTDEF(dwarf_attr)
