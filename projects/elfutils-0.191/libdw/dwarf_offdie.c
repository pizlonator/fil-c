/* Return DIE at given offset.
   Copyright (C) 2002-2010, 2017 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <string.h>
#include "libdwP.h"


Dwarf_Die *
internal_function
__libdw_offdie (Dwarf *dbg, Dwarf_Off offset, Dwarf_Die *result,
		bool debug_types)
{
  if (dbg == NULL)
    return NULL;

  Elf_Data *const data = dbg->sectiondata[debug_types ? IDX_debug_types
					  : IDX_debug_info];
  if (data == NULL || offset >= data->d_size)
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  /* Clear the entire DIE structure.  This signals we have not yet
     determined any of the information.  */
  memset (result, '\0', sizeof (Dwarf_Die));

  result->addr = (char *) data->d_buf + offset;

  /* Get the CU.  */
  result->cu = __libdw_findcu (dbg, offset, debug_types);
  if (result->cu == NULL)
    {
      /* This should never happen.  The input file is malformed.  */
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      result = NULL;
    }

  return result;
}


Dwarf_Die *
dwarf_offdie (Dwarf *dbg, Dwarf_Off offset, Dwarf_Die *result)
{
  return __libdw_offdie (dbg, offset, result, false);
}
INTDEF(dwarf_offdie)

Dwarf_Die *
dwarf_offdie_types (Dwarf *dbg, Dwarf_Off offset, Dwarf_Die *result)
{
  return __libdw_offdie (dbg, offset, result, true);
}
