/* Get abbreviation at given offset.
   Copyright (C) 2004, 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

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


int
dwarf_offabbrev (Dwarf *dbg, Dwarf_Off offset, size_t *lengthp,
		 Dwarf_Abbrev *abbrevp)
{
  if (dbg == NULL)
    return -1;

  Dwarf_Abbrev *abbrev = __libdw_getabbrev (dbg, NULL, offset, lengthp,
					    abbrevp);

  if (abbrev == NULL)
    return -1;

  return abbrev == DWARF_END_ABBREV ? 1 : 0;
}
