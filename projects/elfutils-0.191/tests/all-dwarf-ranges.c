/* Test program for dwarf_ranges
   Copyright (C) 2015, 2018 Red Hat, Inc.
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

#include <config.h>
#include ELFUTILS_HEADER(dw)
#include <dwarf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

static void
ranges_die (Dwarf_Die *die)
{
  Dwarf_Addr base, start, end;
  int ranges = dwarf_ranges (die, 0, &base, &start, &end);
  if (ranges < 0)
    puts (dwarf_errmsg (-1));
  else if (ranges > 0)
    {
      printf ("die: %s (%x)\n", dwarf_diename (die) ?: "<unknown>",
	      dwarf_tag (die));
      for (ptrdiff_t off = 0;
	   (off = dwarf_ranges (die, off, &base, &start, &end)); )
	if (off == -1)
	  {
	    puts (dwarf_errmsg (-1));
	    break;
	  }
	else
	  printf (" %"PRIx64"..%"PRIx64"\n", start, end);
      printf ("\n");
    }
}

static void
walk_tree (Dwarf_Die *dwarf_die)
{
  Dwarf_Die die = *dwarf_die;
  do
    {
      Dwarf_Die child;
      ranges_die (&die);
      if (dwarf_child (&die, &child) == 0)
	walk_tree (&child);
    }
  while (dwarf_siblingof (&die, &die) == 0);
}

int
main (int argc, char *argv[])
{
  assert (argc >= 2);
  const char *name = argv[1];

  int fd = open (name, O_RDONLY);
  Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);

  Dwarf_CU *cu = NULL;
  Dwarf_Die cudie, subdie;
  uint8_t unit_type;
  while (dwarf_get_units (dbg, cu, &cu, NULL,
			  &unit_type, &cudie, &subdie) == 0)
    {
      Dwarf_Die die = (unit_type == DW_UT_skeleton
		       ? subdie : cudie);
      walk_tree (&die);
    }
  dwarf_end (dbg);

  return 0;
}
