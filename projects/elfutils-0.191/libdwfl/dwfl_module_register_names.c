/* Enumerate DWARF register numbers and their names.
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


int
dwfl_module_register_names (Dwfl_Module *mod,
			    int (*func) (void *, int, const char *,
					 const char *, const char *,
					 int, int),
			    void *arg)
{
  if (unlikely (mod == NULL))
    return -1;

  if (unlikely (mod->ebl == NULL))
    {
      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error != DWFL_E_NOERROR)
	{
	  __libdwfl_seterrno (error);
	  return -1;
	}
    }

  int nregs = ebl_register_info (mod->ebl, -1, NULL, 0,
				 NULL, NULL, NULL, NULL);
  int result = 0;
  for (int regno = 0; regno < nregs && likely (result == 0); ++regno)
    {
      char name[32];
      const char *setname = NULL;
      const char *prefix = NULL;
      int bits = -1;
      int type = -1;
      ssize_t len = ebl_register_info (mod->ebl, regno, name, sizeof name,
				       &prefix, &setname, &bits, &type);
      if (unlikely (len < 0))
	{
	  __libdwfl_seterrno (DWFL_E_LIBEBL);
	  result = -1;
	  break;
	}
      if (likely (len > 0))
	{
	  assert (len > 1);	/* Backend should never yield "".  */
	  result = (*func) (arg, regno, setname, prefix, name, bits, type);
	}
    }

  return result;
}
