/* Get address range which includes given address.
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

#include <libdwP.h>


Dwarf_Arange *
dwarf_getarange_addr (Dwarf_Aranges *aranges, Dwarf_Addr addr)
{
  if (aranges == NULL)
    return NULL;

  /* The ranges are sorted by address, so we can use binary search.  */
  size_t l = 0, u = aranges->naranges;
  while (l < u)
    {
      size_t idx = (l + u) / 2;
      if (addr < aranges->info[idx].addr)
	u = idx;
      else if (addr > aranges->info[idx].addr
	       && addr - aranges->info[idx].addr >= aranges->info[idx].length)
	l = idx + 1;
      else
	return &aranges->info[idx];
    }

  __libdw_seterrno (DWARF_E_NO_MATCH);
  return NULL;
}
INTDEF(dwarf_getarange_addr)
