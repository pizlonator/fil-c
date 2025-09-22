/* Test program for dwfl_report_elf incorrect BASE alignment.
   Copyright (C) 2013 Red Hat, Inc.
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
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include ELFUTILS_HEADER(dwfl)
#include "system.h"


static const Dwfl_Callbacks offline_callbacks =
  {
    .find_debuginfo = INTUSE(dwfl_standard_find_debuginfo),
    .section_address = INTUSE(dwfl_offline_section_address),
  };


int
main (int argc, char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  if (argc != 5)
    error (1, 0, "dwfl-report-elf-align shlib.so base funcaddr funcname");
    
  Dwfl *dwfl = dwfl_begin (&offline_callbacks);
  assert (dwfl != NULL);

  char *endptr;
  uintptr_t base = strtoull (argv[2], &endptr, 0);
  assert (endptr && !*endptr);

  Dwfl_Module *mod = dwfl_report_elf (dwfl, argv[1], argv[1], -1, base, false);
  assert (mod != NULL);

  uintptr_t funcaddr = strtoull (argv[3], &endptr, 0);
  assert (endptr && !*endptr);

  Dwfl_Module *mod_found = dwfl_addrmodule (dwfl, funcaddr);
  assert (mod_found == mod);

  const char *symname = dwfl_module_addrname (mod, funcaddr);
  assert (symname != NULL);
  assert (strcmp (symname, argv[4]) == 0);

  dwfl_end (dwfl);

  return 0;
}
