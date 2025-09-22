/* Get line number of beginning of given function.
   Copyright (C) 2005, 2009 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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

#include <assert.h>
#include <dwarf.h>
#include <limits.h>
#include "libdwP.h"


NEW_VERSION (dwarf_decl_line, ELFUTILS_0.143)
int
dwarf_decl_line (Dwarf_Die *func, int *linep)
{
  return __libdw_attr_intval (func, linep, DW_AT_decl_line);
}
OLD_VERSION (dwarf_decl_line, ELFUTILS_0.122)


int internal_function
__libdw_attr_intval (Dwarf_Die *die, int *linep, int attval)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Word line;

  int res = INTUSE(dwarf_formudata) (INTUSE(dwarf_attr_integrate)
				     (die, attval, &attr_mem),
				     &line);
  if (res == 0)
    {
      if (line > INT_MAX)
	{
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  res = -1;
	}
      else
	*linep = line;
    }

  return res;
}
