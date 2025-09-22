/* Test program for dwarf_peel_type. Peels type of top-level vars.
   Copyright (C) 2017 Red Hat, Inc.
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

#include "../libdw/known-dwarf.h"

static const char *
dwarf_tag_string (unsigned int tag)
{
  switch (tag)
    {
#define DWARF_ONE_KNOWN_DW_TAG(NAME, CODE) case CODE: return #NAME;
      DWARF_ALL_KNOWN_DW_TAG
#undef DWARF_ONE_KNOWN_DW_TAG
    default:
      return NULL;
    }
}

void
print_var_raw_type (Dwarf_Die *var)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Die type_mem;
  Dwarf_Die *type;
  const char *name = dwarf_diename (var);

  type = dwarf_formref_die (dwarf_attr (var, DW_AT_type, &attr_mem),
			    &type_mem);
  if (type != NULL)
    {
      /* Test twice, once with a separate result DIE. Then with the
	 DIE itself. The resulting tag should be the same. */
      Dwarf_Die result_mem;
      Dwarf_Die *result = &result_mem;
      int res = dwarf_peel_type (type, result);
      if (res < 0)
        printf ("%s error peeling type: %s\n", name, dwarf_errmsg (-1));
      else if (res > 0)
	printf ("%s missing DW_TAG_TYPE, could peel further: %s\n",
		name, dwarf_tag_string (dwarf_tag (result)));
      else
	{
	  int tag = dwarf_tag (result);
	  printf ("%s raw type %s\n", name, dwarf_tag_string (tag));
	  res = dwarf_peel_type (type, type);
	  if (res < 0)
	    printf ("%s cannot peel type itself: %s\n", name,
		    dwarf_errmsg (-1));
	  else if (res > 0)
	printf ("%s missing DW_TAG_TYPE, could peel type further: %s\n",
		name, dwarf_tag_string (dwarf_tag (type)));
	  else if (dwarf_tag (type) != tag)
	    printf ("%s doesn't resolve the same: %s != %s\n", name,
		    dwarf_tag_string (tag),
		    dwarf_tag_string (dwarf_tag (type)));
	}
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
	    print_var_raw_type (die);

	  if (dwarf_siblingof (die, &die_mem) != 0)
	    break;
	}
    }

  dwfl_end (dwfl);
}
