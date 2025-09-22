/* Return segment type name.
   Copyright (C) 2001, 2002, 2004 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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

#include <stdio.h>
#include <libeblP.h>


const char *
ebl_segment_type_name (Ebl *ebl, int segment, char *buf, size_t len)
{
  const char *res;

  res = ebl != NULL ? ebl->segment_type_name (segment, buf, len) : NULL;
  if (res == NULL)
    {
      static const char *ptypes[PT_NUM] =
	{
#define PTYPE(name) [PT_##name] = #name
	  PTYPE (NULL),
	  PTYPE (LOAD),
	  PTYPE (DYNAMIC),
	  PTYPE (INTERP),
	  PTYPE (NOTE),
	  PTYPE (SHLIB),
	  PTYPE (PHDR),
	  PTYPE (TLS)
	};

      /* Is it one of the standard segment types?  */
      if (segment >= PT_NULL && segment < PT_NUM)
	res = ptypes[segment];
      else if (segment == PT_GNU_EH_FRAME)
	res = "GNU_EH_FRAME";
      else if (segment == PT_GNU_STACK)
	res = "GNU_STACK";
      else if (segment == PT_GNU_RELRO)
	res = "GNU_RELRO";
      else if (segment == PT_GNU_PROPERTY)
	res = "GNU_PROPERTY";
      else if (segment == PT_SUNWBSS)
	res = "SUNWBSS";
      else if (segment == PT_SUNWSTACK)
	res = "SUNWSTACK";
      else
	{
	  if (segment >= PT_LOOS && segment <= PT_HIOS)
	    snprintf (buf, len, "LOOS+%d", segment - PT_LOOS);
	  else if (segment >= PT_LOPROC && segment <= PT_HIPROC)
	    snprintf (buf, len, "LOPROC+%d", segment - PT_LOPROC);
	  else
	    snprintf (buf, len, "%s: %d", _("<unknown>"), segment);

	  res = buf;
	}
    }

  return res;
}
