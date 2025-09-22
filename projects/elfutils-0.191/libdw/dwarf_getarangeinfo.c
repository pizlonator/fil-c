/* Return list address ranges.
   Copyright (C) 2000, 2001, 2002, 2004, 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <libdwP.h>


int
dwarf_getarangeinfo (Dwarf_Arange *arange, Dwarf_Addr *addrp,
		     Dwarf_Word *lengthp, Dwarf_Off *offsetp)
{
  if (arange == NULL)
    return -1;

  if (addrp != NULL)
    *addrp = arange->addr;
  if (lengthp != NULL)
    *lengthp = arange->length;
  if (offsetp != NULL)
    *offsetp = arange->offset;

  return 0;
}
INTDEF(dwarf_getarangeinfo)
