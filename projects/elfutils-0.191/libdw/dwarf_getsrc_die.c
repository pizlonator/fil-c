/* Find line information for address.
   Copyright (C) 2004, 2005, 2014 Red Hat, Inc.
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
#include <assert.h>


Dwarf_Line *
dwarf_getsrc_die (Dwarf_Die *cudie, Dwarf_Addr addr)
{
  Dwarf_Lines *lines;
  size_t nlines;

  if (INTUSE(dwarf_getsrclines) (cudie, &lines, &nlines) != 0)
    return NULL;

  /* The lines are sorted by address, so we can use binary search.  */
  if (nlines > 0)
    {
      size_t l = 0, u = nlines - 1;
      while (l < u)
	{
	  size_t idx = u - (u - l) / 2;
	  Dwarf_Line *line = &lines->info[idx];
	  if (addr < line->addr)
	    u = idx - 1;
	  else
	    l = idx;
	}

      /* This is guaranteed for us by libdw read_srclines.  */
      assert (lines->info[nlines - 1].end_sequence);

      /* The last line which is less than or equal to addr is what we
	 want, unless it is the end_sequence which is after the
	 current line sequence.  */
      Dwarf_Line *line = &lines->info[l];
      if (! line->end_sequence && line->addr <= addr)
	return &lines->info[l];
    }

  __libdw_seterrno (DWARF_E_ADDR_OUTOFRANGE);
  return NULL;
}
