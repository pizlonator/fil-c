/* Register names and numbers for Alpha DWARF.
   Copyright (C) 2007 Red Hat, Inc.
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

#define BACKEND alpha_
#include "libebl_CPU.h"

ssize_t
alpha_register_info (Ebl *ebl __attribute__ ((unused)),
		     int regno, char *name, size_t namelen,
		     const char **prefix, const char **setname,
		     int *bits, int *type)
{
  if (name == NULL)
    return 67;

  if (regno < 0 || regno > 66 || namelen < 7)
    return -1;

  *prefix = "$";

  *bits = 64;
  *type = DW_ATE_signed;
  *setname = "integer";
  if (regno >= 32 && regno < 64)
    {
      *setname = "FPU";
      *type = DW_ATE_float;
    }

  switch (regno)
    {
    case 0:
      name[0] = 'v';
      name[1] = '0';
      namelen = 2;
      break;

    case 1 ... 8:
      name[0] = 't';
      name[1] = regno - 1 + '0';
      namelen = 2;
      break;

    case 9 ... 15:
      name[0] = 's';
      name[1] = regno - 9 + '0';
      namelen = 2;
      break;

    case 16 ... 21:
      name[0] = 'a';
      name[1] = regno - 16 + '0';
      namelen = 2;
      break;

    case 22 ... 23:
      name[0] = 't';
      name[1] = regno - 22 + '8';
      namelen = 2;
      break;

    case 24 ... 25:
      name[0] = 't';
      name[1] = '1';
      name[2] = regno - 24 + '0';
      namelen = 3;
      break;

    case 26:
      *type = DW_ATE_address;
      return stpcpy (name, "ra") + 1 - name;

    case 27:
      return stpcpy (name, "t12") + 1 - name;

    case 28:
      return stpcpy (name, "at") + 1 - name;

    case 29:
      *type = DW_ATE_address;
      return stpcpy (name, "gp") + 1 - name;

    case 30:
      *type = DW_ATE_address;
      return stpcpy (name, "sp") + 1 - name;

    case 31:
      return stpcpy (name, "zero") + 1 - name;

    case 32 ... 32 + 9:
      name[0] = 'f';
      name[1] = regno - 32 + '0';
      namelen = 2;
      break;

    case 32 + 10 ... 32 + 19:
      name[0] = 'f';
      name[1] = '1';
      name[2] = regno - 32 - 10 + '0';
      namelen = 3;
      break;

    case 32 + 20 ... 32 + 29:
      name[0] = 'f';
      name[1] = '2';
      name[2] = regno - 32 - 20 + '0';
      namelen = 3;
      break;

    case 32 + 30:
      return stpcpy (name, "f30") + 1 - name;

    case 32 + 31:
      *type = DW_ATE_unsigned;
      return stpcpy (name, "fpcr") + 1 - name;

    case 64:
      *type = DW_ATE_address;
      return stpcpy (name, "pc") + 1 - name;

    case 66:
      *type = DW_ATE_address;
      return stpcpy (name, "unique") + 1 - name;

    default:
      *setname = NULL;
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
