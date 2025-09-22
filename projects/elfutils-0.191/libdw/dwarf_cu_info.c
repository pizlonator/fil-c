/* Provides information and DIEs associated with the Dwarf_CU unit.
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
#include "libdwP.h"


int
dwarf_cu_info (Dwarf_CU *cu,
	       Dwarf_Half *version, uint8_t *unit_type,
	       Dwarf_Die *cudie, Dwarf_Die *subdie,
	       uint64_t *unit_id,
	       uint8_t *address_size, uint8_t *offset_size)
{
  if (cu == NULL)
    return -1;

  if (version != NULL)
    *version = cu->version;

  if (unit_type != NULL)
    *unit_type = cu->unit_type;

  if (cudie != NULL)
    {
      if (cu->version >= 2 && cu->version <= 5
	  && cu->unit_type >= DW_UT_compile
	  && cu->unit_type <= DW_UT_split_type)
	*cudie = CUDIE (cu);
      else
	{
	invalid:
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}
    }

  if (subdie != NULL)
    {
      if (cu->version >= 2 && cu->version <= 5)
	{
	  /* For types, return the actual type DIE.  For skeletons,
	     find the associated split compile unit and return its
	     DIE.  */
	  if (cu->unit_type == DW_UT_type
	      || cu->unit_type == DW_UT_split_type)
	    *subdie = SUBDIE(cu);
	  else if (cu->unit_type == DW_UT_skeleton)
	    {
	      Dwarf_CU *split_cu = __libdw_find_split_unit (cu);
	      if (split_cu != NULL)
		*subdie = CUDIE(split_cu);
	      else
		memset (subdie, '\0', sizeof (Dwarf_Die));
	    }
	  else
	    memset (subdie, '\0', sizeof (Dwarf_Die));
	}
      else
	goto invalid;
    }

  if (unit_id != NULL)
    *unit_id = cu->unit_id8;

  if (address_size != NULL)
    *address_size = cu->address_size;

  if (offset_size != NULL)
    *offset_size = cu->offset_size;

  return 0;
}
