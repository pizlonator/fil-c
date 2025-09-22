/* Fetch source line info for CU.
   Copyright (C) 2005, 2006 Red Hat, Inc.
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

Dwfl_Error
internal_function
__libdwfl_cu_getsrclines (struct dwfl_cu *cu)
{
  if (cu->lines == NULL)
    {
      Dwarf_Lines *lines;
      size_t nlines;
      if (INTUSE(dwarf_getsrclines) (&cu->die, &lines, &nlines) != 0)
	return DWFL_E_LIBDW;

      cu->lines = malloc (offsetof (struct Dwfl_Lines, idx[nlines]));
      if (cu->lines == NULL)
	return DWFL_E_NOMEM;
      cu->lines->cu = cu;
      for (unsigned int i = 0; i < nlines; ++i)
	cu->lines->idx[i].idx = i;
    }

  return DWFL_E_NOERROR;
}
