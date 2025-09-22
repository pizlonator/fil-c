/* Test all DW_LANG constants are handled by dwarf_default_lower_bound.

   Copyright (C) 2016 Red Hat, Inc.
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
#include "../libdw/known-dwarf.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static void
test_lang (const char *name, int lang)
{
  Dwarf_Sword low;
  int res = dwarf_default_lower_bound (lang, &low);

  /* Assembler is special, it doesn't really have arrays.  */
  if (lang == DW_LANG_Mips_Assembler)
    {
      if (res == 0)
	{
	  printf ("%s shouldn't have a known lower bound\n", name);
	  exit (-1);
	}
      printf ("%s: <unknown>\n", name);
      return;
    }

  if (res != 0)
    {
      printf ("dwarf_default_lower_bound failed (%d) for %s\n", res, name);
      exit (-1);
    }

  /* All currently known lower bounds are either zero or one, but
     they don't have to.  Update test once one is a different value.  */
  if (low != 0 && low != 1)
    {
      printf ("unexpected lower bound %" PRId64 " for %s\n", low, name);
      exit (-1);
    }

  printf ("%s: %" PRId64 "\n", name, low);
}

int
main (int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
  Dwarf_Sword low;
  /* Bad language code must fail.  */
  if (dwarf_default_lower_bound (-1, &low) == 0)
    {
      printf ("Bad lang code -1 succeeded (%" PRId64 ")\n", low);
      exit (-1);
    }

  /* Test all known language codes.  */
#define DWARF_ONE_KNOWN_DW_LANG(NAME, CODE) test_lang (#NAME, CODE);
  DWARF_ALL_KNOWN_DW_LANG
#undef DWARF_ONE_KNOWN_DW_LANG

  return 0;
}
