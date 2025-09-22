/* Copyright (C) 2011 Red Hat, Inc.
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
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int
main (int argc, char **argv)
{
  assert (argc > 1);

  int i = open (argv[1], O_RDONLY);
  assert (i >= 0);

  Dwarf *dw = dwarf_begin (i, DWARF_C_READ);
  assert (dw != NULL);

  Dwarf_Die die_mem, *die;
  die = dwarf_offdie (dw, 11, &die_mem);
  assert (die == &die_mem);
  assert (dwarf_tag (die) == 0);

  die = dwarf_offdie (dw, 11, &die_mem);
  assert (die == &die_mem);
  assert (dwarf_tag (die) == 0);

  dwarf_end (dw);
  return 0;
}
