/* Test program for dwarf_decl_file
   Copyright (c) 2024 Meta Platforms, Inc. and affiliates.
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
#include <stdio.h>
#include <unistd.h>

#include <dwarf.h>
#include ELFUTILS_HEADER(dw)

static void
walk_tree (Dwarf_Die *dwarf_die, int indent)
{
  Dwarf_Die die = *dwarf_die;
  do
    {
      int child_indent = indent;
      const char *file = dwarf_decl_file (&die);
      if (file != NULL)
	{
	  printf("%*s", indent, "");
	  const char *name = dwarf_diename (&die) ?: "???";
	  int line, column;
	  if (dwarf_decl_line (&die, &line) == 0)
	    {
	      if (dwarf_decl_column (&die, &column) == 0)
		printf ("%s@%s:%d:%d\n", name, file, line, column);
	      else
		printf ("%s@%s:%d\n", name, file, line);
	    }
	  else
	    printf ("%s@%s\n", name, file);
	  child_indent++;
	}

      Dwarf_Die child;
      if (dwarf_child (&die, &child) == 0)
	walk_tree (&child, child_indent);
    }
  while (dwarf_siblingof (&die, &die) == 0);
}

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
      while (dwarf_get_units (dbg, cu, &cu, NULL, &unit_type, &cudie, &subdie)
	     == 0)
	{
	  Dwarf_Die *die = unit_type == DW_UT_skeleton ? &subdie : &cudie;
	  printf (" cu: %s\n", dwarf_diename (die) ?: "???");
	  walk_tree (die, 2);
	}

      dwarf_end (dbg);
      close (fd);
    }

  return 0;
}
