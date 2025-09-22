/* Register names and numbers for x86-64 DWARF.
   Copyright (C) 2005, 2006, 2007 Red Hat, Inc.
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

#include <assert.h>
#include <dwarf.h>
#include <string.h>

#define BACKEND x86_64_
#include "libebl_CPU.h"

ssize_t
x86_64_register_info (Ebl *ebl __attribute__ ((unused)),
		      int regno, char *name, size_t namelen,
		      const char **prefix, const char **setname,
		      int *bits, int *type)
{
  if (name == NULL)
    return 67;

  if (regno < 0 || regno > 66 || namelen < 7)
    return -1;

  *prefix = "%";
  *bits = 64;
  *type = DW_ATE_unsigned;
  if (regno < 17)
    {
      *setname = "integer";
      *type = DW_ATE_signed;
    }
  else if (regno < 33)
    {
      *setname = "SSE";
      *bits = 128;
    }
  else if (regno < 41)
    {
      *setname = "x87";
      *type = DW_ATE_float;
      *bits = 80;
    }
  else if (regno < 49)
    *setname = "MMX";
  else if (regno > 49 && regno < 60)
    {
      *setname = "segment";
      *bits = 16;
    }
  else
    *setname = "control";

  switch (regno)
    {
      static const char baseregs[][2] =
	{
	  "ax", "dx", "cx", "bx", "si", "di", "bp", "sp"
	};

    case 6 ... 7:
      *type = DW_ATE_address;
      FALLTHROUGH;
    case 0 ... 5:
      name[0] = 'r';
      name[1] = baseregs[regno][0];
      name[2] = baseregs[regno][1];
      namelen = 3;
      break;

    case 8 ... 9:
      name[0] = 'r';
      name[1] = regno - 8 + '8';
      namelen = 2;
      break;

    case 10 ... 15:
      name[0] = 'r';
      name[1] = '1';
      name[2] = regno - 10 + '0';
      namelen = 3;
      break;

    case 16:
      *type = DW_ATE_address;
      name[0] = 'r';
      name[1] = 'i';
      name[2] = 'p';
      namelen = 3;
      break;

    case 17 ... 26:
      name[0] = 'x';
      name[1] = 'm';
      name[2] = 'm';
      name[3] = regno - 17 + '0';
      namelen = 4;
      break;

    case 27 ... 32:
      name[0] = 'x';
      name[1] = 'm';
      name[2] = 'm';
      name[3] = '1';
      name[4] = regno - 27 + '0';
      namelen = 5;
      break;

    case 33 ... 40:
      name[0] = 's';
      name[1] = 't';
      name[2] = regno - 33 + '0';
      namelen = 3;
      break;

    case 41 ... 48:
      name[0] = 'm';
      name[1] = 'm';
      name[2] = regno - 41 + '0';
      namelen = 3;
      break;

    case 50 ... 55:
      name[0] = "ecsdfg"[regno - 50];
      name[1] = 's';
      namelen = 2;
      break;

    case 58 ... 59:
      *type = DW_ATE_address;
      *bits = 64;
      name[0] = regno - 58 + 'f';
      return stpcpy (&name[1], "s.base") + 1 - name;

    case 49:
      *setname = "integer";
      return stpcpy (name, "rflags") + 1 - name;
    case 62:
      return stpcpy (name, "tr") + 1 - name;
    case 63:
      return stpcpy (name, "ldtr") + 1 - name;
    case 64:
      return stpcpy (name, "mxcsr") + 1 - name;

    case 65 ... 66:
      *bits = 16;
      name[0] = 'f';
      name[1] = "cs"[regno - 65];
      name[2] = 'w';
      namelen = 3;
      break;

    default:
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
