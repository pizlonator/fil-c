/* Register names and numbers for C-SKY DWARF.
   Copyright (C) 2019 Hangzhou C-SKY Microsystems co.,ltd.
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

#define BACKEND csky_
#include "libebl_CPU.h"

ssize_t
csky_register_info (Ebl *ebl  __attribute__ ((unused)),
		    int regno, char *name, size_t namelen,
		    const char **prefix, const char **setname,
		    int *bits, int *type)
{
  if (name == NULL)
    return 38;

  *prefix = "";
  *bits = 32;
  *type = DW_ATE_signed;
  *setname = "integer";

  switch (regno)
    {
    case 0 ... 9:
      name[0] = 'r';
      name[1] = regno + '0';
      namelen = 2;
      break;

    case 10 ... 13:
    case 16 ... 30:
      name[0] = 'r';
      name[1] = regno / 10 + '0';
      name[2] = regno % 10 + '0';
      namelen = 3;
      break;

    case 14:
      stpcpy (name, "sp");
      namelen = 2;
      break;

    case 15:
      stpcpy (name, "lr");
      namelen = 2;
      break;

    case 31:
      stpcpy (name, "tls");
      namelen = 3;
      break;

    case 36:
      stpcpy (name, "hi");
      namelen = 2;
      break;

    case 37:
      stpcpy (name, "lo");
      namelen = 2;
      break;

    default:
      *setname = NULL;
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
