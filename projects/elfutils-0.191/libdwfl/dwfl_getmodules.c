/* Iterate through modules.
   Copyright (C) 2005, 2008 Red Hat, Inc.
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

ptrdiff_t
dwfl_getmodules (Dwfl *dwfl,
		 int (*callback) (Dwfl_Module *, void **,
				  const char *, Dwarf_Addr, void *),
		 void *arg,
		 ptrdiff_t offset)
{
  if (dwfl == NULL)
    return -1;

  /* We iterate through the linked list when it's all we have.
     But continuing from an offset is slow that way.  So when
     DWFL->lookup_module is populated, we can instead keep our
     place by jumping directly into the array.  Since the actions
     of a callback could cause it to get populated, we must
     choose the style of place-holder when we return an offset,
     and we encode the choice in the low bits of that value.  */

  Dwfl_Module *m = dwfl->modulelist;

  if ((offset & 3) == 1)
    {
      offset >>= 2;
      for (ptrdiff_t pos = 0; pos < offset; ++pos)
	if (m == NULL)
	  return -1;
	else
	  m = m->next;
    }
  else if (((offset & 3) == 2) && likely (dwfl->lookup_module != NULL))
    {
      offset >>= 2;

      if ((size_t) offset - 1 == dwfl->lookup_elts)
	return 0;

      if (unlikely ((size_t) offset - 1 > dwfl->lookup_elts))
	return -1;

      m = dwfl->lookup_module[offset - 1];
      if (unlikely (m == NULL))
	return -1;
    }
  else if (offset != 0)
    {
      __libdwfl_seterrno (DWFL_E_BADSTROFF);
      return -1;
    }

  while (m != NULL)
    {
      int ok = (*callback) (MODCB_ARGS (m), arg);
      ++offset;
      m = m->next;
      if (ok != DWARF_CB_OK)
	return ((dwfl->lookup_module == NULL) ? ((offset << 2) | 1)
		: (((m == NULL ? (ptrdiff_t) dwfl->lookup_elts + 1
		     : m->segment + 1) << 2) | 2));
    }
  return 0;
}
INTDEF (dwfl_getmodules)
