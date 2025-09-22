/* Generate tables for x86 disassembler.
   Copyright (C) 2007, 2008 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2007.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system.h"

extern int i386_parse (void);


extern FILE *i386_in;
extern int i386_debug;
char *infname;

FILE *outfile;

int
main (int argc, char *argv[argc])
{
  outfile = stdout;

  if (argc == 1)
    error (EXIT_FAILURE, 0, "usage: %s <MNEDEFFILE>", argv[0]);

  //i386_debug = 1;
  infname = argv[1];
  if (strcmp (infname, "-") == 0)
    i386_in = stdin;
  else
    {
      i386_in = fopen (infname, "r");
      if (i386_in == NULL)
	error (EXIT_FAILURE, errno, "cannot open %s", argv[1]);
    }

  i386_parse ();

  return error_message_count != 0;
}
