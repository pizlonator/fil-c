/* Register names and numbers for SPARC DWARF.
   Copyright (C) 2005, 2006, 2015 Red Hat, Inc.
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

#include <string.h>
#include <dwarf.h>

#define BACKEND sparc_
#include "libebl_CPU.h"

ssize_t
sparc_register_info (Ebl *ebl,
		     int regno, char *name, size_t namelen,
		     const char **prefix, const char **setname,
		     int *bits, int *type)
{
  const int nfp = 32 + (ebl->class == ELFCLASS32 ? 0 : 16);
  const int nspec = ebl->class == ELFCLASS32 ? 8 : 6;

  if (name == NULL)
    return 32 + nfp + nspec;

  if (regno < 0 || regno >= 32 + nfp + nspec || namelen < 6)
    return -1;

  *bits = ebl->class == ELFCLASS32 ? 32 : 64;
  *type = DW_ATE_signed;

  *prefix = "%";

  if (regno >= 32 + nfp)
    {
      regno -= 32 + nfp;
      static const char names[2][8][6] =
	{
	  { "y", "psr", "wim", "tbr", "pc", "npc", "fsr", "csr" }, /* v8 */
	  { "pc", "npc", "state", "fsr", "fprs", "y" } /* v9 */
	};
      *setname = "control";
      *type = DW_ATE_unsigned;
      if ((ebl->class == ELFCLASS64 ? 0 : 4) + 1 - (unsigned int) regno <= 1)
	*type = DW_ATE_address;
      return stpncpy (name, names[ebl->class == ELFCLASS64][regno],
		      namelen) + 1 - name;
    }

  if (regno < 32)
    {
      *setname = "integer";
      name[0] = "goli"[regno >> 3];
      name[1] = (regno & 7) + '0';
      namelen = 2;
      if ((regno & 8) && (regno & 7) == 6)
	*type = DW_ATE_address;
    }
  else
    {
      *setname = "FPU";
      *type = DW_ATE_float;

      regno -= 32;
      if (regno >= 32)
	regno = 32 + 2 * (regno - 32);
      else
	*bits = 32;

      name[0] = 'f';
      if (regno < 10)
	{
	  name[1] = regno + '0';
	  namelen = 2;
	}
      else
	{
	  name[1] = regno / 10 + '0';
	  name[2] = regno % 10 + '0';
	  namelen = 3;
	}
    }

  name[namelen++] = '\0';
  return namelen;
}
