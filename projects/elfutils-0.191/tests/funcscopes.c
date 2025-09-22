/* Test program for dwarf_getscopes.
   Copyright (C) 2005, 2014 Red Hat, Inc.
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


static void
paddr (const char *prefix, Dwarf_Addr addr, Dwfl_Line *line)
{
  const char *src;
  int lineno, linecol;
  if (line != NULL
      && (src = dwfl_lineinfo (line, &addr, &lineno, &linecol,
			       NULL, NULL)) != NULL)
    {
      if (linecol != 0)
	printf ("%s%#" PRIx64 " (%s:%d:%d)",
		prefix, addr, src, lineno, linecol);
      else
	printf ("%s%#" PRIx64 " (%s:%d)",
		prefix, addr, src, lineno);
    }
  else
    printf ("%s%#" PRIx64, prefix, addr);
}


static void
print_vars (unsigned int indent, Dwarf_Die *die)
{
  Dwarf_Die child;
  if (dwarf_child (die, &child) == 0)
    do
      switch (dwarf_tag (&child))
	{
	case DW_TAG_variable:
	case DW_TAG_formal_parameter:
	  printf ("%*s%-30s[%6" PRIx64 "]\n", indent, "",
		  dwarf_diename (&child),
		  (uint64_t) dwarf_dieoffset (&child));
	  break;
	default:
	  break;
	}
    while (dwarf_siblingof (&child, &child) == 0);

  Dwarf_Attribute attr_mem;
  Dwarf_Die origin;
  if (dwarf_hasattr (die, DW_AT_abstract_origin)
      && dwarf_formref_die (dwarf_attr (die, DW_AT_abstract_origin, &attr_mem),
			    &origin) != NULL
      && dwarf_child (&origin, &child) == 0)
    do
      switch (dwarf_tag (&child))
	{
	case DW_TAG_variable:
	case DW_TAG_formal_parameter:
	  printf ("%*s%s (abstract)\n", indent, "",
		  dwarf_diename (&child));
	  break;
	default:
	  break;
	}
    while (dwarf_siblingof (&child, &child) == 0);
}


#define INDENT 4

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

  Dwarf_Die *scopes;
  int n = dwarf_getscopes_die (funcdie, &scopes);
  if (n <= 0)
    error (EXIT_FAILURE, 0, "dwarf_getscopes_die: %s", dwarf_errmsg (-1));
  else
    {
      Dwarf_Addr start, end;
      const char *fname;
      const char *modname = dwfl_module_info (dwfl_cumodule (a->cu), NULL,
					      &start, &end,
					      NULL, NULL,
					      &fname, NULL);
      if (modname == NULL)
	error (EXIT_FAILURE, 0, "dwfl_module_info: %s", dwarf_errmsg (-1));
      if (modname[0] == '\0')
	modname = fname;
      printf ("%s: %#" PRIx64 " .. %#" PRIx64 "\n", modname, start, end);

      unsigned int indent = 0;
      while (n-- > 0)
	{
	  Dwarf_Die *const die = &scopes[n];

	  indent += INDENT;
	  printf ("%*s%s (%#x)", indent, "",
		  dwarf_diename (die) ?: "<unnamed>",
		  dwarf_tag (die));

	  Dwarf_Addr lowpc, highpc;
	  if (dwarf_lowpc (die, &lowpc) == 0
	      && dwarf_highpc (die, &highpc) == 0)
	    {
	      lowpc += a->dwbias;
	      highpc += a->dwbias;
	      Dwfl_Line *loline = dwfl_getsrc (a->dwfl, lowpc);
	      Dwfl_Line *hiline = dwfl_getsrc (a->dwfl, highpc - 1);
	      paddr (": ", lowpc, loline);
	      if (highpc != lowpc)
		paddr (" .. ", highpc - 1, hiline == loline ? NULL : hiline);
	    }
	  puts ("");

	  print_vars (indent + INDENT, die);
	}
      free (scopes);
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
