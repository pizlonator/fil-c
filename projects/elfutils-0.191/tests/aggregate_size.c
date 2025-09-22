/* Test program for dwarf_aggregate_size. Prints size of top-level vars.
   Copyright (C) 2014 Red Hat, Inc.
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

#include <assert.h>
#include <argp.h>
#include <inttypes.h>
#include <fcntl.h>
#include ELFUTILS_HEADER(dw)
#include ELFUTILS_HEADER(dwfl)
#include <stdio.h>
#include <unistd.h>
#include <dwarf.h>

void
print_var_type_size (Dwarf_Die *var)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Die type_mem;
  Dwarf_Die *type;
  const char *name = dwarf_diename (var);

  type = dwarf_formref_die (dwarf_attr (var, DW_AT_type, &attr_mem),
			    &type_mem);
  if (type != NULL)
    {
      Dwarf_Word size;
      if (dwarf_aggregate_size (type, &size) < 0)
        printf ("%s no size: %s\n", name, dwarf_errmsg (-1));
      else
	printf ("%s size %" PRIu64 "\n", name, size);
    }
  else
    printf ("%s has no type.\n", name);
}

int
main (int argc, char *argv[])
{

  int remaining;
  Dwfl *dwfl;
  (void) argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining,
                     &dwfl);
  assert (dwfl != NULL);

  Dwarf_Die *cu = NULL;
  Dwarf_Addr dwbias;
  while ((cu = dwfl_nextcu (dwfl, cu, &dwbias)) != NULL)
    {
      Dwarf_Die die_mem;
      Dwarf_Die *die = &die_mem;
      dwarf_child (cu, &die_mem);

      while (1)
	{
	  if (dwarf_tag (die) == DW_TAG_variable)
	    print_var_type_size (die);

	  if (dwarf_siblingof (die, &die_mem) != 0)
	    break;
	}
    }

  dwfl_end (dwfl);
}
