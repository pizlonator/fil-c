/* Copyright (C) 2012, 2013, 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <fcntl.h>
#include ELFUTILS_HEADER(dw)
#include <stdio.h>
#include <unistd.h>
#include <dwarf.h>
#include <inttypes.h>

int
main (int argc, char *argv[])
{
  for (int i = 1; i < argc; ++i)
    {
      int fd = open (argv[i], O_RDONLY);

      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if (dbg != NULL)
	{
	  Dwarf_Off off = 0;
	  size_t cuhl;
	  Dwarf_Off noff;
	  uint64_t type_sig;

	  while (dwarf_next_unit (dbg, off, &noff, &cuhl, NULL, NULL, NULL,
				  NULL, &type_sig, NULL) == 0)
	    {
	      Dwarf_Die die_mem;
	      dwarf_offdie_types (dbg, off + cuhl, &die_mem);
	      off = noff;
	    }

	  off = 0;

	  while (dwarf_nextcu (dbg, off, &noff, &cuhl, NULL, NULL, NULL) == 0)
	    {
	      Dwarf_Die die_mem;
	      Dwarf_Die *die = dwarf_offdie (dbg, off + cuhl, &die_mem);

	      Dwarf_Die iter_mem;
	      Dwarf_Die *iter = &iter_mem;
	      dwarf_child (die, &iter_mem);

	      while (1)
		{
		  if (dwarf_tag (iter) == DW_TAG_variable)
		    {
		      Dwarf_Attribute attr_mem;
		      Dwarf_Die form_mem, *form;
		      form = dwarf_formref_die (dwarf_attr (iter, DW_AT_type,
							    &attr_mem),
						&form_mem);

		      if (form == NULL)
			printf ("fail\n");
		      else
			printf ("ok %s [%" PRIx64 "]\n",
				dwarf_diename (form), dwarf_dieoffset (form));
		    }

		  if (dwarf_siblingof (iter, &iter_mem) != 0)
		    break;
		}

	      off = noff;
	    }

	  dwarf_end (dbg);
	}

      close (fd);
    }
}
