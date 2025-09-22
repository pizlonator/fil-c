/* Test program for dwarf_ranges
   Copyright (C) 2015 Red Hat, Inc.
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

int
main (int argc, char *argv[])
{
  assert (argc >= 3);
  const char *name = argv[1];
  ptrdiff_t cuoff = strtol (argv[2], NULL, 0);

  int fd = open (name, O_RDONLY);
  Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);

  Dwarf_Die cudie_mem, *cudie = dwarf_offdie (dbg, cuoff, &cudie_mem);

  Dwarf_Addr base, start, end;
  for (ptrdiff_t off = 0;
       (off = dwarf_ranges (cudie, off, &base, &start, &end)); )
    if (off == -1)
      {
	puts (dwarf_errmsg (dwarf_errno ()));
	break;
      }
    else
      fprintf (stderr, "%"PRIx64"..%"PRIx64" (base %"PRIx64")\n",
	       start, end, base);

  dwarf_end (dbg);

  return 0;
}
