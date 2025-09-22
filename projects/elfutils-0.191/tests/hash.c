/* Copyright (C) 2002 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <libelf.h>
#include <stdio.h>


static int
check (const char *name, unsigned long int expected)
{
  unsigned long int actual = elf_hash (name);

  return actual != expected;
}


int
main (void)
{
  int status;

  /* Check some names.  We know what the expected result is.  */
  status = check ("_DYNAMIC", 165832675);
  status |= check ("_GLOBAL_OFFSET_TABLE_", 102264335);

  return status;
}
