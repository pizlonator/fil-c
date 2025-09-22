/* Register names and numbers for SH DWARF.
   Copyright (C) 2010 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Matt Fleming <matt@console-pimps.org>.

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
#include <string.h>

#define BACKEND sh_
#include "libebl_CPU.h"

ssize_t
sh_register_info (Ebl *ebl __attribute__ ((unused)),
		  int regno, char *name, size_t namelen,
		  const char **prefix, const char **setname,
		  int *bits, int *type)
{
  if (name == NULL)
    return 104;

  if (regno < 0 || regno > 103 || namelen < 6)
    return -1;

  *prefix = "";
  *bits = 32;
  *type = DW_ATE_signed;

  switch (regno)
    {
    case 0 ... 9:
      *setname = "integer";
      name[0] = 'r';
      name[1] = regno + '0';
      namelen = 2;
      break;

    case 10 ... 15:
      *setname = "integer";
      name[0] = 'r';
      name[1] = '1';
      name[2] = regno - 10 + '0';
      namelen = 3;
      break;

    case 16:
      *setname = "system";
      *type = DW_ATE_address;
      name[0] = 'p';
      name[1] = 'c';
      namelen = 2;
      break;

    case 17:
      *setname = "system";
      *type = DW_ATE_address;
      name[0] = 'p';
      name[1] = 'r';
      namelen = 2;
      break;

    case 18:
      *setname = "control";
      *type = DW_ATE_unsigned;
      name[0] = 's';
      name[1] = 'r';
      namelen = 2;
      break;

    case 19:
      *setname = "control";
      *type = DW_ATE_unsigned;
      name[0] = 'g';
      name[1] = 'b';
      name[2] = 'r';
      namelen = 3;
      break;

    case 20:
      *setname = "system";
      name[0] = 'm';
      name[1] = 'a';
      name[2] = 'c';
      name[3] = 'h';
      namelen = 4;
      break;

    case 21:
      *setname = "system";
      name[0] = 'm';
      name[1] = 'a';
      name[2] = 'c';
      name[3] = 'l';
      namelen = 4;

      break;

    case 23:
      *setname = "system";
      *type = DW_ATE_unsigned;
      name[0] = 'f';
      name[1] = 'p';
      name[2] = 'u';
      name[3] = 'l';
      namelen = 4;
      break;

    case 24:
      *setname = "system";
      *type = DW_ATE_unsigned;
      name[0] = 'f';
      name[1] = 'p';
      name[2] = 's';
      name[3] = 'c';
      name[4] = 'r';
      namelen = 5;
      break;

    case 25 ... 34:
      *setname = "fpu";
      *type = DW_ATE_float;
      name[0] = 'f';
      name[1] = 'r';
      name[2] = regno - 25 + '0';
      namelen = 3;
      break;

    case 35 ... 40:
      *setname = "fpu";
      *type = DW_ATE_float;
      name[0] = 'f';
      name[1] = 'r';
      name[2] = '1';
      name[3] = regno - 35 + '0';
      namelen = 4;
      break;

    case 87 ... 96:
      *type = DW_ATE_float;
      *setname = "fpu";
      name[0] = 'x';
      name[1] = 'f';
      name[2] = regno - 87 + '0';
      namelen = 3;
      break;

    case 97 ... 103:
      *type = DW_ATE_float;
      *setname = "fpu";
      name[0] = 'x';
      name[1] = 'f';
      name[2] = '1';
      name[3] = regno - 97 + '0';
      namelen = 4;
      break;

    default:
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
