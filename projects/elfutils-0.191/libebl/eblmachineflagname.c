/* Return machine flag names.
   Copyright (C) 2001, 2002 Red Hat, Inc.
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
#include <string.h>
#include <system.h>
#include <libeblP.h>


const char *
ebl_machine_flag_name (Ebl *ebl, Elf64_Word flags, char *buf, size_t len)
{
  const char *res;

  if (flags == 0)
    res = "";
  else
    {
      Elf64_Word orig_flags = flags;
      char *cp = buf;
      bool first = true;
      const char *machstr;
      size_t machstrlen;

      do
	{
	  if (! first)
	    {
	      if (cp + 2 >= buf + len)
		break;
	      *cp++ = ',';
	      *cp++ = ' ';
	    }

	  machstr = ebl != NULL ? ebl->machine_flag_name (orig_flags, &flags) : NULL;
	  if (machstr == NULL)
	    {
	      /* No more known flag.  */
	      snprintf (cp, buf + len - cp, "%#x", flags);
	      break;
	    }

	  machstrlen = strlen (machstr) + 1;
	  if ((size_t) (buf + len - cp) < machstrlen)
	    {
	      *((char *) mempcpy (cp, machstr, buf + len - cp - 1)) = '\0';
	      break;
	    }

	  cp = mempcpy (cp, machstr, machstrlen);
	  --cp;

	  first = false;
	}
      while (flags != 0);

      res = buf;
    }

  return res;
}
