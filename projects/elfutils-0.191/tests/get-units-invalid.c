/* Test cudie and subdie properties.
   Copyright (C) 2018 Red Hat, Inc.
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

#include <dwarf.h>
#include ELFUTILS_HEADER(dw)
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  for (int i = 1; i < argc; i++)
    {
      printf ("file: %s\n", argv[i]);
      int fd = open (argv[i], O_RDONLY);
      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if (dbg == NULL)
	{
	  printf ("%s not usable: %s\n", argv[i], dwarf_errmsg (-1));
	  return -1;
	}

      Dwarf_CU *cu = NULL;
      Dwarf_Die cudie, subdie;
      uint8_t unit_type;
      while (dwarf_get_units (dbg, cu, &cu, NULL,
			      &unit_type, &cudie, &subdie) == 0)
	{
	  printf ("Got cudie: %s, unit_type: %" PRIx8 "\n",
		  dwarf_diename (&cudie), unit_type);

	  int tag = dwarf_tag (&subdie);
	  if (unit_type == DW_UT_compile)
	    {
	      if (tag != DW_TAG_invalid)
		{
		  printf ("Not invalid: %x\n", dwarf_tag (&subdie));
		  return -1;
		}
	      if (dwarf_diename (&subdie) != NULL)
		{
		  printf ("Should have NULL name: %s\n",
			  dwarf_diename (&subdie));
		  return -1;
		}
	      Dwarf_Die result;
	      if (dwarf_siblingof (&subdie, &result) != -1)
		{
		  printf ("Should NOT have a valid sibling: %s\n",
			  dwarf_diename (&result));
		  return -1;
		}
	      if (dwarf_child (&subdie, &result) != -1)
		{
		  printf ("Should NOT have a valid child: %s\n",
			  dwarf_diename (&result));
		  return -1;
		}
	      Dwarf_Addr base, start, end;
	      if (dwarf_ranges (&subdie, 0, &base, &start, &end) != -1)
		{
		  printf ("Should NOT have a ranges: %s\n",
			  dwarf_diename (&subdie));
		  return -1;
		}
	      if (dwarf_cuoffset (&subdie) != (Dwarf_Off) -1)
		{
		  printf ("Should NOT have a cuoffset: %s\n",
			  dwarf_diename (&subdie));
		  return -1;
		}
	      if (dwarf_dieoffset (&subdie) != (Dwarf_Off) -1)
		{
		  printf ("Should NOT have a die offset: %s\n",
			  dwarf_diename (&subdie));
		  return -1;
		}
	      if (dwarf_getabbrev (&subdie, 0, NULL) != NULL)
		{
		  printf ("Should NOT have an abbrev: %s\n",
			  dwarf_diename (&subdie));
		  return -1;
		}
	    }
	  else if (unit_type == DW_UT_type)
	    printf ("subdie: %s\n", dwarf_diename (&subdie));
	  else
	    printf ("subdie tag: %x\n", dwarf_tag (&subdie));
	}

      dwarf_end (dbg);
      close (fd);

      printf ("\n");
    }

  return 0;
}
