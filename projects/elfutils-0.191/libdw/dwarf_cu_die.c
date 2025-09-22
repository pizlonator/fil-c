/* Internal definitions for libdwarf.
   Copyright (C) 2014 Red Hat, Inc.
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

#include <stddef.h>
#include "libdwP.h"


Dwarf_Die *
dwarf_cu_die (Dwarf_CU *cu, Dwarf_Die *result, Dwarf_Half *versionp,
	      Dwarf_Off *abbrev_offsetp, uint8_t *address_sizep,
	      uint8_t *offset_sizep, uint64_t *unit_idp,
	      Dwarf_Off *subdie_offsetp)
{
  if (cu == NULL)
    return NULL;

  *result = CUDIE (cu);

  if (versionp != NULL)
    *versionp = cu->version;
  if (abbrev_offsetp != NULL)
    *abbrev_offsetp = cu->orig_abbrev_offset;
  if (address_sizep != NULL)
    *address_sizep = cu->address_size;
  if (offset_sizep != NULL)
    *offset_sizep = cu->offset_size;
  if (unit_idp != NULL)
    *unit_idp = cu->unit_id8;
  if (subdie_offsetp != NULL)
    *subdie_offsetp = cu->subdie_offset;

  return result;
}
