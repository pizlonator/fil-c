/* Test program for dwarf_getmacros and related
   Copyright (C) 2009, 2014 Red Hat, Inc.
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

static void include (Dwarf *dbg, Dwarf_Off macoff, ptrdiff_t token);

static int
mac (Dwarf_Macro *macro, void *dbg)
{
  static int level = 0;

  unsigned int opcode;
  dwarf_macro_opcode (macro, &opcode);
  switch (opcode)
    {
    case DW_MACRO_import:
      {
	Dwarf_Attribute at;
	int r = dwarf_macro_param (macro, 0, &at);
	assert (r == 0);

	Dwarf_Word w;
	r = dwarf_formudata (&at, &w);
	assert (r == 0);

	printf ("%*sinclude %#" PRIx64 "\n", level, "", w);
	++level;
	include (dbg, w, DWARF_GETMACROS_START);
	--level;
	printf ("%*s/include\n", level, "");
	break;
      }

    case DW_MACRO_start_file:
      {
	Dwarf_Files *files;
	size_t nfiles;
	if (dwarf_macro_getsrcfiles (dbg, macro, &files, &nfiles) < 0)
	  printf ("dwarf_macro_getsrcfiles: %s\n",
		  dwarf_errmsg (dwarf_errno ()));

	Dwarf_Word w = 0;
	dwarf_macro_param2 (macro, &w, NULL);

	const char *name = dwarf_filesrc (files, (size_t) w, NULL, NULL);
	printf ("%*sfile %s\n", level, "", name);
	++level;
	break;
      }

    case DW_MACRO_end_file:
      {
	--level;
	printf ("%*s/file\n", level, "");
	break;
      }

    case DW_MACINFO_define:
    case DW_MACRO_define_strp:
    case DW_MACRO_define_sup:
    case DW_MACRO_define_strx:
      {
	const char *value;
	dwarf_macro_param2 (macro, NULL, &value);
	printf ("%*s%s\n", level, "", value);
	break;
      }

    case DW_MACINFO_undef:
    case DW_MACRO_undef_strp:
    case DW_MACRO_undef_sup:
    case DW_MACRO_undef_strx:
      break;

    default:
      {
	size_t paramcnt;
	dwarf_macro_getparamcnt (macro, &paramcnt);
	printf ("%*sopcode %u with %zd arguments\n",
		level, "", opcode, paramcnt);
	break;
      }
    }

  return DWARF_CB_ABORT;
}

static void
include (Dwarf *dbg, Dwarf_Off macoff, ptrdiff_t token)
{
  while ((token = dwarf_getmacros_off (dbg, macoff, mac, dbg, token)) != 0)
    if (token == -1)
      {
	puts (dwarf_errmsg (dwarf_errno ()));
	break;
      }
}

static void
getmacros (Dwarf *dbg, Dwarf_Die *die, bool new_style)
{
  for (ptrdiff_t off = new_style ? DWARF_GETMACROS_START : 0;
       (off = dwarf_getmacros (die, mac, dbg, off)); )
    if (off == -1)
      {
	puts (dwarf_errmsg (-1));
	break;
      }
}

int
main (int argc, char *argv[])
{
  assert (argc >= 3);
  const char *name = argv[1];
  bool new_style = argc > 3;

  int fd = open (name, O_RDONLY);
  Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);

  if (argv[2][0] == '\0')
    {
      Dwarf_CU *cu = NULL;
      Dwarf_Die cudie, subdie;
      uint8_t unit_type;
      while (dwarf_get_units (dbg, cu, &cu, NULL,
			      &unit_type, &cudie, &subdie) == 0)
	{
	  Dwarf_Die *die = (unit_type == DW_UT_skeleton
			    ? &subdie : &cudie);
	  if (! dwarf_hasattr (die, DW_AT_macro_info)
	      && ! dwarf_hasattr (die, DW_AT_GNU_macros)
	      && ! dwarf_hasattr (die, DW_AT_macros))
	    continue;
	  printf ("CU %s\n", dwarf_diename (die));
	  getmacros (dbg, die, new_style);
	}
    }
  else
    {
      ptrdiff_t cuoff = strtol (argv[2], NULL, 0);
      Dwarf_Die cudie_mem, *cudie = dwarf_offdie (dbg, cuoff, &cudie_mem);
      if (cudie == NULL)
	{
	  puts (dwarf_errmsg (-1));
	  return 1;
	}
      getmacros (dbg, cudie, new_style);
    }

  dwarf_end (dbg);

  return 0;
}
