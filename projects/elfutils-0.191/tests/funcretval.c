/* Test program for dwfl_module_return_value_location.
   Copyright (C) 2005 Red Hat, Inc.
   Copyright (C) 2023 Mark J. Wielaard <mark@klomp.org>
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
#include <assert.h>
#include <inttypes.h>
#include ELFUTILS_HEADER(dwfl)
#include <dwarf.h>
#include <argp.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include "system.h"


struct args
{
  Dwfl *dwfl;
  Dwarf_Die *cu;
  Dwarf_Addr dwbias;
  char **argv;
};

static int
handle_function (Dwarf_Die *funcdie, void *arg)
{
  struct args *a = arg;

  const char *name = dwarf_diename (funcdie);
  char **argv = a->argv;
  if (argv[0] != NULL)
    {
      bool match;
      do
	match = fnmatch (*argv, name, 0) == 0;
      while (!match && *++argv);
      if (!match)
	return 0;
    }

  printf ("(%s) %s: ", dwfl_module_info (dwfl_cumodule (a->cu), NULL,
					 NULL, NULL,
					 NULL, NULL,
					 NULL, NULL), name);

  const Dwarf_Op *locops;
  int nlocops = dwfl_module_return_value_location (dwfl_cumodule (a->cu),
						   funcdie, &locops);
  if (nlocops < 0)
    error (EXIT_FAILURE, 0, "dwfl_module_return_value_location: %s",
	   dwfl_errmsg (-1));
  else if (nlocops == 0)
    {
      // Check if this is the special unspecified type
      // https://sourceware.org/bugzilla/show_bug.cgi?id=30047
      Dwarf_Die die_mem, *typedie = &die_mem;
      Dwarf_Attribute attr_mem, *attr;
      attr = dwarf_attr_integrate (funcdie, DW_AT_type, &attr_mem);
      if (dwarf_formref_die (attr, typedie) != NULL
	  && dwarf_tag (typedie) == DW_TAG_unspecified_type)
	puts ("returns unspecified type");
      else
	puts ("returns no value");
    }
  else
    {
      printf ("return value location:");
      for (int i = 0; i < nlocops; ++i)
	printf (" {%#x, %#" PRIx64 "}", locops[i].atom, locops[i].number);
      puts ("");
    }

  return 0;
}


int
main (int argc, char *argv[])
{
  int remaining;

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  struct args a = { .dwfl = NULL, .cu = NULL };

  (void) argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining,
		     &a.dwfl);
  assert (a.dwfl != NULL);
  a.argv = &argv[remaining];

  int result = 0;

  while ((a.cu = dwfl_nextcu (a.dwfl, a.cu, &a.dwbias)) != NULL)
    dwarf_getfuncs (a.cu, &handle_function, &a, 0);

  dwfl_end (a.dwfl);

  return result;
}
