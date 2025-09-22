/* Test dwarf_cu_info properties.
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

/* Yeah, lazy, 16K CUs should be enough for everybody... */
#define MAX_UNITS 16384
struct info
{
  int dietag;
  int subtag;
  Dwarf_Half version;
  uint8_t unit_type;
  uint64_t id;
  uint8_t addr_size;
  uint8_t off_size;
};
static struct info unit_info[MAX_UNITS];

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
      Dwarf_Half version;
      Dwarf_Die cudie, subdie;
      uint8_t unit_type;
      size_t u, units;
      u = units = 0;
      printf ("Iterate getting all info, compare with dwarf_cu_info.\n");
      while (dwarf_get_units (dbg, cu, &cu, &version,
			      &unit_type, &cudie, &subdie) == 0)
	{
	  int dietag = dwarf_tag (&cudie);
	  int subtag = dwarf_tag (&subdie);

	  unit_info[u].dietag = dietag;
	  unit_info[u].subtag = subtag;
	  unit_info[u].version = version;
	  unit_info[u].unit_type = unit_type;

	  printf ("%zu cu dietag: %x, subtag: %x, version %" PRIx32
		  ", unit_type %" PRIx8 "\n",
		  u, dietag, subtag, version, unit_type);

	  uint64_t unit_id;
	  uint8_t addr_size, off_size;
	  if (dwarf_cu_info (cu,
			     &version, &unit_type, &cudie, &subdie,
			     &unit_id, &addr_size, &off_size) != 0)
	    {
	      printf ("Invalid dwarf_cu_info: %s\n", dwarf_errmsg (-1));
	      return -1;
	    }

	  dietag = dwarf_tag (&cudie);
	  subtag = dwarf_tag (&subdie);

	  if (unit_info[u].dietag != dietag)
	    {
	      printf("Unequal dietags\n");
	      return -1;
	    }

	  if (unit_info[u].subtag != subtag)
	    {
	      printf("Unequal subtags\n");
	      return -1;
	    }

	  if (unit_info[u].version != version)
	    {
	      printf("Unequal versions\n");
	      return -1;
	    }

	  if (unit_info[u].unit_type != unit_type)
	    {
	      printf("Unequal unit_types\n");
	      return -1;
	    }

	  unit_info[u].id = unit_id;
	  unit_info[u].addr_size = addr_size;
	  unit_info[u].off_size = off_size;

	  if (unit_type == DW_UT_skeleton)
	    {
	      if (dwarf_cu_info (subdie.cu,
				 &version, &unit_type, &cudie, &subdie,
				 &unit_id, &addr_size, &off_size) != 0)
		{
		  printf ("Invalid subdie dwarf_cu_info: %s\n",
			  dwarf_errmsg (-1));
		  return -1;
		}

	      dietag = dwarf_tag (&cudie);
	      subtag = dwarf_tag (&subdie);

	      printf ("%zu subdietag: %x, subtag: %x, version %" PRIx32
		      ", unit_type %" PRIx8 "\n",
		      u, dietag, subtag, version, unit_type);

	      /* subdie is now cudie.  */
	      if (unit_info[u].subtag != dietag)
	      {
		printf ("Inconsistent subdie tag\n");
		return -1;
	      }

	      if (unit_info[u].id != unit_id)
		{
		  printf ("Unequal subdie ids\n");
		  return -1;
		}

	      if (unit_info[u].addr_size != addr_size)
		{
		  printf ("Unequal subdie addr_size\n");
		  return -1;
		}

	      if (unit_info[u].off_size != off_size)
		{
		  printf ("Unequal subdie off_size\n");
		  return -1;
		}
	    }

	  if (u >= MAX_UNITS)
	    {
	      printf ("Oops, more than 16K units...\n");
	      return -1;
	    }
	  u = ++units;
	}

      dwarf_end (dbg);
      close (fd);

      /* And again... */
      printf ("rechecking: %s\n", argv[i]);
      fd = open (argv[i], O_RDONLY);
      dbg = dwarf_begin (fd, DWARF_C_READ);
      if (dbg == NULL)
	{
	  printf ("%s not usable: %s\n", argv[i], dwarf_errmsg (-1));
	  return -1;
	}

      cu = NULL;
      u = 0;
      printf ("Iterate no info, compare recorded info with dwarf_cu_info.\n");
      while (dwarf_get_units (dbg, cu, &cu, NULL, NULL, NULL, NULL) == 0)
	{
	  if (u > units)
	    {
	      printf ("Got too many units???\n");
	      return -1;
	    }

	  uint64_t unit_id;
	  uint8_t addr_size, off_size;
	  if (dwarf_cu_info (cu,
			     &version, &unit_type, &cudie, &subdie,
			     &unit_id, &addr_size, &off_size) != 0)
	    {
	      printf ("Invalid dwarf_cu_info: %s\n", dwarf_errmsg (-1));
	      return -1;
	    }

	  int dietag = dwarf_tag (&cudie);
	  int subtag = dwarf_tag (&subdie);

	  printf ("%zu re dietag: %x, subtag: %x, version %" PRIx32
		  ", unit_type %" PRIx8 "\n",
		  u, dietag, subtag, version, unit_type);

	  if (unit_info[u].dietag != dietag)
	    {
	      printf("Unequal dietags %x != %x\n", unit_info[u].dietag, dietag);
	      return -1;
	    }

	  if (unit_info[u].subtag != subtag)
	    {
	      printf("Unequal subtags\n");
	      return -1;
	    }

	  if (unit_info[u].version != version)
	    {
	      printf("Unequal versions\n");
	      return -1;
	    }

	  if (unit_info[u].unit_type != unit_type)
	    {
	      printf("Unequal unit_types\n");
	      return -1;
	    }

	  if (unit_info[u].id != unit_id)
	    {
	      printf ("Unequal subdie ids\n");
	      return -1;
	    }

	  if (unit_info[u].addr_size != addr_size)
	    {
	      printf ("Unequal subdie addr_size\n");
	      return -1;
	    }

	  if (unit_info[u].off_size != off_size)
	    {
	      printf ("Unequal subdie off_size\n");
	      return -1;
	    }

	  if (unit_type == DW_UT_skeleton)
	    {
	      if (dwarf_cu_info (subdie.cu,
				 &version, &unit_type, &cudie, &subdie,
				 &unit_id, &addr_size, &off_size) != 0)
		{
		  printf ("Invalid subdie dwarf_cu_info: %s\n",
			  dwarf_errmsg (-1));
		  return -1;
		}

	      dietag = dwarf_tag (&cudie);
	      subtag = dwarf_tag (&subdie);

	      printf ("%zu subdietag: %x, subtag: %x, version %" PRIx32
		      ", unit_type %" PRIx8 "\n",
		      u, dietag, subtag, version, unit_type);

	      /* subdie is now cudie.  */
	      subtag = dwarf_tag (&cudie);
	      if (unit_info[u].subtag != subtag)
	      {
		printf ("Inconsistent subdie tag\n");
		return -1;
	      }

	      if (unit_info[u].id != unit_id)
		{
		  printf ("Unequal subdie ids\n");
		  return -1;
		}

	      if (unit_info[u].addr_size != addr_size)
		{
		  printf ("Unequal subdie addr_size\n");
		  return -1;
		}

	      if (unit_info[u].off_size != off_size)
		{
		  printf ("Unequal subdie off_size\n");
		  return -1;
		}
	    }

	  if (u >= MAX_UNITS)
	    {
	      printf ("Oops, more than 16K units...\n");
	      return -1;
	    }
	  u++;
	}

      if (u != units)
	{
	  printf ("Got not enough units???\n");
	  return -1;
	}

      dwarf_end (dbg);
      close (fd);

      printf ("\n");
    }

  return 0;
}
