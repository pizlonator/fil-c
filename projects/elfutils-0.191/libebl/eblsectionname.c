/* Return section name.
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
ebl_section_name (Ebl *ebl, int section, int xsection, char *buf, size_t len,
		  const char *scnnames[], size_t shnum)
{
  const char *res = ebl != NULL ? ebl->section_name (section, xsection,
						     buf, len) : NULL;

  if (res == NULL)
    {
      if (section == SHN_UNDEF)
	res = "UNDEF";
      else if (section == SHN_ABS)
	res = "ABS";
      else if (section == SHN_COMMON)
	res = "COMMON";
      else if (section == SHN_BEFORE)
	res = "BEFORE";
      else if (section == SHN_AFTER)
	res = "AFTER";
      else if ((section < SHN_LORESERVE || section == SHN_XINDEX)
	       && (size_t) section < shnum)
	{
	  int idx = section != SHN_XINDEX ? section : xsection;

	  if (scnnames != NULL)
	    res = scnnames[idx];
	  else
	    {
	      snprintf (buf, len, "%d", idx);
	      res = buf;
	    }
	}
      else
	{
	  /* Handle OS-specific section names.  */
	  if (section == SHN_XINDEX)
	    snprintf (buf, len, "%s: %d", "XINDEX", xsection);
	  else if (section >= SHN_LOOS && section <= SHN_HIOS)
	    snprintf (buf, len, "LOOS+%x", section - SHN_LOOS);
	  /* Handle processor-specific section names.  */
	  else if (section >= SHN_LOPROC && section <= SHN_HIPROC)
	    snprintf (buf, len, "LOPROC+%x", section - SHN_LOPROC);
	  else if (section >= SHN_LORESERVE && section <= SHN_HIRESERVE)
	    snprintf (buf, len, "LORESERVE+%x", section - SHN_LORESERVE);
	  else
	    snprintf (buf, len, "%s: %d", _("<unknown>"), section);

	  res = buf;
	}
    }

  return res;
}
