/* Return offset of DIE.
   Copyright (C) 2018 Red Hat, Inc.
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

#include <string.h>

#include <dwarf.h>
#include "libdwP.h"


Dwarf_Die *
dwarf_die_addr_die (Dwarf *dbg, void *addr, Dwarf_Die *result)
{
  if (dbg == NULL)
    return NULL;

  Dwarf_CU *cu = __libdw_findcu_addr (dbg, addr);

  if (cu == NULL)
    {
      Dwarf *alt = INTUSE (dwarf_getalt) (dbg);
      if (alt != NULL)
	cu = __libdw_findcu_addr (alt, addr);
    }

  if (cu == NULL)
    {
      Dwarf *split = __libdw_find_split_dbg_addr (dbg, addr);
      if (split != NULL)
	cu = __libdw_findcu_addr (split, addr);
    }

  if (cu == NULL)
    {
      memset (result, '\0', sizeof (Dwarf_Die));
      return NULL;
    }

  *result = (Dwarf_Die) { .addr = addr, .cu = cu };

  return result;
}
