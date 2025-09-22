/* Test program for dwfl_getmodules bug.
   Copyright (C) 2008 Red Hat, Inc.
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
#include ELFUTILS_HEADER(dwfl)
#include "system.h"


static const Dwfl_Callbacks callbacks =
  {
    .find_elf = dwfl_linux_proc_find_elf,
    .find_debuginfo = dwfl_standard_find_debuginfo,
  };

static int
iterate (Dwfl_Module *mod __attribute__ ((unused)),
	 void **userdata __attribute__ ((unused)),
	 const char *name __attribute__ ((unused)),
	 Dwarf_Addr base, void *arg)
{
  if (base != 0x2000)
    return DWARF_CB_OK;

  if (dwfl_addrmodule (arg, 0x2100) == NULL)
    error (1, 0, "dwfl_addrmodule: %s", dwfl_errmsg (-1));

  return DWARF_CB_ABORT;
}

int
main (void)
{
  Dwfl *dwfl = dwfl_begin (&callbacks);

  dwfl_report_module (dwfl, "m1", 0, 0x1000);
  dwfl_report_module (dwfl, "m2", 0x2000, 0x3000);
  dwfl_report_module (dwfl, "m3", 0x4000, 0x5000);

  dwfl_report_end (dwfl, NULL, NULL);

  ptrdiff_t offset = dwfl_getmodules (dwfl, &iterate, dwfl, 0);
  if (offset <= 0)
    error (1, 0, "dwfl_getmodules: %s", dwfl_errmsg (-1));

  offset = dwfl_getmodules (dwfl, &iterate, NULL, offset);
  if (offset != 0)
    error (1, 0, "dwfl_getmodules (%d): %s", (int) offset, dwfl_errmsg (-1));

  dwfl_end (dwfl);

  return 0;
}
