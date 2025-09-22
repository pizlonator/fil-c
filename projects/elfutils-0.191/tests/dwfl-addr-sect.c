/* Test program for libdwfl ... foo
   Copyright (C) 2007 Red Hat, Inc.
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
#include <sys/types.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <argp.h>
#include ELFUTILS_HEADER(dwfl)
#include <dwarf.h>
#include "system.h"

static int
handle_address (Dwfl *dwfl, Dwarf_Addr address)
{
  Dwfl_Module *mod = dwfl_addrmodule (dwfl, address);
  Dwarf_Addr adjusted = address;
  Dwarf_Addr bias;
  Elf_Scn *scn = dwfl_module_address_section (mod, &adjusted, &bias);
  if (scn == NULL)
    {
      error (0, 0, "%#" PRIx64 ": dwfl_module_address_section: %s",
	     address, dwfl_errmsg (-1));
      return 1;
    }
  printf ("address %#" PRIx64 " => module \"%s\" section %zu + %#" PRIx64 "\n",
	  address,
	  dwfl_module_info (mod, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	  elf_ndxscn (scn), adjusted);
  return 0;
}

int
main (int argc, char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  int remaining;
  Dwfl *dwfl = NULL;
  (void) argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining, &dwfl);
  assert (dwfl != NULL);

  int result = 0;
  for (; remaining < argc; ++remaining)
    {
      char *endp;
      uintmax_t addr = strtoumax (argv[remaining], &endp, 0);
      if (endp != argv[remaining])
	result |= handle_address (dwfl, addr);
      else
	result = 1;
    }

  dwfl_end (dwfl);

  return result;
}
