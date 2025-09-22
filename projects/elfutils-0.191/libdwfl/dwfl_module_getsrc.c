/* Find source location for PC address in module.
   Copyright (C) 2005, 2008, 2014 Red Hat, Inc.
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

#include "libdwflP.h"
#include "libdwP.h"

Dwfl_Line *
dwfl_module_getsrc (Dwfl_Module *mod, Dwarf_Addr addr)
{
  Dwarf_Addr bias;
  if (INTUSE(dwfl_module_getdwarf) (mod, &bias) == NULL)
    return NULL;

  struct dwfl_cu *cu;
  Dwfl_Error error = __libdwfl_addrcu (mod, addr, &cu);
  if (likely (error == DWFL_E_NOERROR))
    error = __libdwfl_cu_getsrclines (cu);
  if (likely (error == DWFL_E_NOERROR))
    {
      Dwarf_Lines *lines = cu->die.cu->lines;
      size_t nlines = lines->nlines;
      if (nlines > 0)
	{
	  /* This is guaranteed for us by libdw read_srclines.  */
	  assert(lines->info[nlines - 1].end_sequence);

	  /* Now we look at the module-relative address.  */
	  addr -= bias;

	  /* The lines are sorted by address, so we can use binary search.  */
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

	  /* The last line which is less than or equal to addr is what
	     we want, unless it is the end_sequence which is after the
	     current line sequence.  */
	  Dwarf_Line *line = &lines->info[l];
	  if (! line->end_sequence && line->addr <= addr)
	    return &cu->lines->idx[l];
	}

      error = DWFL_E_ADDR_OUTOFRANGE;
    }

  __libdwfl_seterrno (error);
  return NULL;
}
INTDEF (dwfl_module_getsrc)
