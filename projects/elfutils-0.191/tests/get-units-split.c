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
      int count = 0;
      while (dwarf_get_units (dbg, cu, &cu, NULL,
			      &unit_type, &cudie, &subdie) == 0)
	{
	  count++;
	  printf ("Got cudie unit_type: %" PRIx8 "\n", unit_type);

	  if (unit_type == DW_UT_skeleton)
	    {
	      Dwarf_CU *skel_cu = cudie.cu;
	      Dwarf_CU *split_cu = subdie.cu;
	      Dwarf_Die skel_die, split_die;
	      uint64_t skel_id, split_id;

	      printf ("Found a skeleton unit, with split die: %s\n",
		      dwarf_diename (&subdie));

	      if (dwarf_cu_die (skel_cu, &skel_die, NULL, NULL, NULL, NULL,
				&skel_id, NULL) == NULL)
		{
		  printf ("bad skel_cu: %s\n", dwarf_errmsg (-1));
		  return -1;
		}

	      if (dwarf_cu_die (split_cu, &split_die, NULL, NULL, NULL, NULL,
				&split_id, NULL) == NULL)
		{
		  printf ("bad skel_cu: %s\n", dwarf_errmsg (-1));
		  return -1;
		}

	      if (skel_id != split_id)
		{
		  printf ("Skeleton id and Split id not equal!\n");
		  return -1;
		}
	    }
	}

      if (count == 0)
	{
	  printf ("No units found\n");
	  return -1;
	}

      dwarf_end (dbg);
      close (fd);

      printf ("\n");
    }

  return 0;
}
