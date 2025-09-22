/* Iterate through the CU units for a given Dwarf.
   Copyright (C) 2016, 2017 Red Hat, Inc.
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
dwarf_get_units (Dwarf *dwarf, Dwarf_CU *cu, Dwarf_CU **next_cu,
		 Dwarf_Half *version, uint8_t *unit_type,
		 Dwarf_Die *cudie, Dwarf_Die *subdie)
{
  /* Handle existing error.  */
  if (dwarf == NULL)
    return -1;

  Dwarf_Off off;
  bool v4type;
  if (cu == NULL)
    {
      off = 0;
      v4type = false;
    }
  else
    {
      off = cu->end;
      v4type = cu->sec_idx != IDX_debug_info;

      /* Make sure we got a real (not fake) CU.  */
      if (cu->sec_idx != IDX_debug_info && cu->sec_idx != IDX_debug_types)
	{
	  __libdw_seterrno (DWARF_E_INVALID_OFFSET);
	  return -1;
	}

      /* Do we have to switch to the other section, or are we at the end?  */
      if (! v4type)
	{
	  if (off >= cu->dbg->sectiondata[IDX_debug_info]->d_size)
	    {
	      if (cu->dbg->sectiondata[IDX_debug_types] == NULL)
		return 1;

	      off = 0;
	      v4type = true;
	    }
	}
      else
	if (off >= cu->dbg->sectiondata[IDX_debug_types]->d_size)
	  return 1;
    }

  *next_cu = __libdw_findcu (dwarf, off, v4type);
  if (*next_cu == NULL)
    return -1;

  Dwarf_CU *next = (*next_cu);

  if (version != NULL)
    *version = next->version;

  if (unit_type != NULL)
    *unit_type = next->unit_type;

  if (cudie != NULL)
    {
      if (next->version >= 2 && next->version <= 5
	  && next->unit_type >= DW_UT_compile
	  && next->unit_type <= DW_UT_split_type)
	*cudie = CUDIE (next);
      else
	memset (cudie, '\0', sizeof (Dwarf_Die));
    }

  if (subdie != NULL)
    {
      if (next->version >= 2 && next->version <= 5)
	{
	  /* For types, return the actual type DIE.  For skeletons,
	     find the associated split compile unit and return its
	     DIE.  */
	  if (next->unit_type == DW_UT_type
	      || next->unit_type == DW_UT_split_type)
	    *subdie = SUBDIE(next);
	  else if (next->unit_type == DW_UT_skeleton)
	    {
	      Dwarf_CU *split_cu = __libdw_find_split_unit (next);
	      if (split_cu != NULL)
		*subdie = CUDIE(split_cu);
	      else
		memset (subdie, '\0', sizeof (Dwarf_Die));
	    }
	  else
	    memset (subdie, '\0', sizeof (Dwarf_Die));
	}
      else
	memset (subdie, '\0', sizeof (Dwarf_Die));
    }

  return 0;
}
INTDEF(dwarf_get_units)
