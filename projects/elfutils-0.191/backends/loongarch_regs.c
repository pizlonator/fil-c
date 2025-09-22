/* Register names and numbers for LoongArch DWARF.
   Copyright (C) 2023 OpenAnolis community LoongArch SIG.
   Copyright (C) 2023 Loongson Technology Corporation Limted.
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

#define BACKEND loongarch_
#include "libebl_CPU.h"

ssize_t
loongarch_register_info (Ebl *ebl, int regno, char *name, size_t namelen,
		     const char **prefix, const char **setname,
		     int *bits, int *type)
{
  if (name == NULL)
    return 64;

  *prefix = "";

  if (regno < 32)
    {
      *setname = "integer";
      *type = DW_ATE_signed;
      *bits = ebl->class == ELFCLASS64 ? 64 : 32;
    }
  else
    {
      *setname = "FPU";
      *type = DW_ATE_float;
      *bits = 64;
    }

  switch (regno)
    {
    case 0:
      return stpcpy (name, "zero") + 1 - name;

    case 1:
      *type = DW_ATE_address;
      return stpcpy (name, "ra") + 1 - name;

    case 2:
      *type = DW_ATE_address;
      return stpcpy (name, "tp") + 1 - name;

    case 3:
      *type = DW_ATE_address;
      return stpcpy (name, "sp") + 1 - name;

    case 4 ... 11:
      name[0] = 'a';
      name[1] = regno - 4 + '0';
      namelen = 2;
      break;

    case 12 ... 20:
      name[0] = 't';
      name[1] = regno - 12 + '0';
      namelen = 2;
      break;

    case 21:
      return stpcpy (name, "u0") + 1 - name;

    case 22:
      *type = DW_ATE_address;
      return stpcpy (name, "fp") + 1 - name;

    case 23 ... 31:
      name[0] = 's';
      name[1] = regno - 23 + '0';
      namelen = 2;
      break;

    case 32 ... 39:
      name[0] = 'f';
      name[1] = 'a';
      name[2] = regno - 32 + '0';
      namelen = 3;
      break;

    case 40 ... 49:
      name[0] = 'f';
      name[1] = 't';
      name[2] = regno - 40 + '0';
      namelen = 3;
      break;

    case 50 ... 55:
      name[0] = 'f';
      name[1] = 't';
      name[2] = '1';
      name[3] = regno - 50 + '0';
      namelen = 4;
      break;

    case 56 ... 63:
      name[0] = 'f';
      name[1] = 's';
      name[2] = regno - 56 + '0';
      namelen = 3;
      break;

    default:
      *setname = NULL;
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
