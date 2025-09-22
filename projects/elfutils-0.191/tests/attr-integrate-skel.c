/* Test dwarf_get_units finds split DWO CUs.
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
      Dwarf_Half version;
      int count = 0;
      while (dwarf_get_units (dbg, cu, &cu, &version,
			      &unit_type, &cudie, &subdie) == 0)
	{
	  count++;
	  if (unit_type == DW_UT_skeleton)
	    {
	      Dwarf_Attribute attr;
	      Dwarf_Word word;
	      Dwarf_Addr addr;

	      printf ("Split DIE: %s\n", dwarf_diename (&subdie));

	      if (dwarf_attr_integrate (&subdie,
					DW_AT_GNU_addr_base, &attr) == NULL
		  && dwarf_attr_integrate (&subdie,
					   DW_AT_addr_base, &attr) == NULL)
		printf ("No addr_base");
	      else if (dwarf_formudata (&attr, &word) != 0)
		printf ("Bad addr_base: %s\n", dwarf_errmsg (-1));
	      else
		printf ("addr_base secoff: 0x%" PRIx64 "\n", word);

	      if (dwarf_attr (&subdie, DW_AT_low_pc, &attr) != NULL)
		printf ("Unexpected low_pc on split DIE.\n");

	      if (dwarf_attr_integrate (&subdie,
					DW_AT_low_pc, &attr) == NULL)
		printf ("No low_pc");
	      else if (dwarf_formaddr (&attr, &addr) != 0)
		printf ("Bad low_pc: %s\n", dwarf_errmsg (-1));
	      else
		printf ("low_pc addr: 0x%" PRIx64 "\n", addr);

	      if (dwarf_hasattr (&subdie, DW_AT_high_pc))
		printf ("Unexpected highpc on split DIE\n");
	      if (dwarf_hasattr (&subdie, DW_AT_ranges))
		printf ("Unexpected ranges on split DIE\n");

	      if (dwarf_hasattr_integrate (&subdie, DW_AT_high_pc))
		printf ("Skel has high_pc.\n");
	      if (dwarf_hasattr_integrate (&subdie, DW_AT_ranges))
		printf ("Skel has ranges.\n");

	      printf ("\n");
	    }
	}

      if (count == 0)
	{
	  printf ("No units found\n");
	  return -1;
	}

      dwarf_end (dbg);
      close (fd);
    }

  return 0;
}
