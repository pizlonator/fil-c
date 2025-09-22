/* Copyright (C) 2014 Red Hat, Inc.
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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libelf.h>
#include ELFUTILS_HEADER(dw)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "system.h"


int
main (int argc, char *argv[])
{
  /* file addr+ */
  int fd = open (argv[1], O_RDONLY);
  Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
  if  (dbg == NULL)
    error (-1, 0, "dwarf_begin (%s): %s\n", argv[1], dwarf_errmsg (-1));

  for (int i = 2; i < argc; i++)
    {
      Dwarf_Addr addr;
      char *endptr;
      Dwarf_Die cudie;
      Dwarf_Line *line;

      errno = 0;
      addr = strtoull (argv[i], &endptr, 16);
      if (errno != 0)
	error (-1, errno, "Cannot parrse '%s'", argv[1]);

      if (dwarf_addrdie (dbg, addr, &cudie) == NULL)
	error (-1, 0, "dwarf_addrdie (%s): %s", argv[i], dwarf_errmsg (-1));

      line = dwarf_getsrc_die (&cudie, addr);
      if (line == NULL)
	error (-1, 0, "dwarf_getsrc_die (%s): %s", argv[i], dwarf_errmsg (-1));

      const char *f = dwarf_linesrc (line, NULL, NULL);
      int l;
      if (dwarf_lineno (line, &l) != 0)
	l = 0;

      printf ("%s:%d\n", f ?: "???", l);
    }

  dwarf_end (dbg);
  close (fd);

  return 0;
}
