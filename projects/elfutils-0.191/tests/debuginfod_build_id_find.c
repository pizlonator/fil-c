/* Test program for fetching debuginfo with debuginfo-server.
   Copyright (C) 2019 Red Hat, Inc.
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
#include <stdio.h>
#include ELFUTILS_HEADER(dwfl)
#include <elf.h>
#include <dwarf.h>
#include <argp.h>
#include <assert.h>
#include <string.h>

static const char *debuginfo_path = "";
static const Dwfl_Callbacks cb  =
  {
    NULL,
    dwfl_standard_find_debuginfo,
    NULL,
    (char **)&debuginfo_path,
  };

int
main (int argc __attribute__ ((unused)), char **argv)
{
  int expect_pass = strcmp(argv[3], "0");
  Dwarf_Addr bias = 0;
  Dwfl *dwfl = dwfl_begin(&cb);
  dwfl_report_begin(dwfl);

  /* Open an executable.  */
  Dwfl_Module *mod = dwfl_report_offline(dwfl, argv[2], argv[2], -1);

  /* The corresponding debuginfo will not be found in debuginfo_path
     (since it's empty), causing the server to be queried.  */

  Dwarf *res = dwfl_module_getdwarf(mod, &bias);
  if (expect_pass)
    assert(res);
  else
    assert(!res);

  dwfl_end (dwfl);

  return 0;
}
