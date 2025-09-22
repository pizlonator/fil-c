/* Peel type aliases and qualifier tags from a type DIE.
   Copyright (C) 2014, 2015, 2016 Red Hat, Inc.
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

#include "libdwP.h"
#include <dwarf.h>
#include <string.h>


int
dwarf_peel_type (Dwarf_Die *die, Dwarf_Die *result)
{
  int tag;

  /* Ignore previous errors.  */
  if (die == NULL)
    return -1;

  *result = *die;
  tag = INTUSE (dwarf_tag) (result);

/* Stack 8 of all these modifiers, after that it gets silly.  */
#define MAX_DEPTH (8 * 8)
  int max_depth = MAX_DEPTH;
  while ((tag == DW_TAG_typedef
	  || tag == DW_TAG_const_type
	  || tag == DW_TAG_volatile_type
	  || tag == DW_TAG_restrict_type
	  || tag == DW_TAG_atomic_type
	  || tag == DW_TAG_immutable_type
	  || tag == DW_TAG_packed_type
	  || tag == DW_TAG_shared_type)
	&& max_depth-- > 0)
    {
      Dwarf_Attribute attr_mem;
      Dwarf_Attribute *attr = INTUSE (dwarf_attr_integrate) (result, DW_AT_type,
							     &attr_mem);
      if (attr == NULL)
	return 1;

      if (INTUSE (dwarf_formref_die) (attr, result) == NULL)
	return -1;

      tag = INTUSE (dwarf_tag) (result);
    }

  if (tag == DW_TAG_invalid || max_depth <= 0)
    return -1;

  return 0;
}
INTDEF(dwarf_peel_type)
