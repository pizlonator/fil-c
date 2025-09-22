/* Check whether given DIE has specific attribute.
   Copyright (C) 2003, 2005, 2014, 2017 Red Hat, Inc.
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


int
dwarf_hasattr (Dwarf_Die *die, unsigned int search_name)
{
  if (die == NULL)
    return 0;

  /* Find the abbreviation entry.  */
  Dwarf_Abbrev *abbrevp = __libdw_dieabbrev (die, NULL);
  if (unlikely (abbrevp == DWARF_END_ABBREV))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return 0;
    }

  /* Search the name attribute.  Dwarf_Abbrev was checked when created,
     so we can read unchecked here.  */
  const unsigned char *attrp = abbrevp->attrp;
  while (1)
    {
      /* Get attribute name and form.  */
      unsigned int attr_name;
      get_uleb128_unchecked (attr_name, attrp);
      unsigned int attr_form;
      get_uleb128_unchecked (attr_form, attrp);

      /* We can stop if we found the end of the attribute list.  */
      if (attr_name == 0 && attr_form == 0)
	return 0;

      if (attr_name == search_name)
	return 1;

      if (attr_form == DW_FORM_implicit_const)
	{
	  int64_t attr_value __attribute__ ((unused));
	  get_sleb128_unchecked (attr_value, attrp);
	}
    }
}
INTDEF (dwarf_hasattr)
