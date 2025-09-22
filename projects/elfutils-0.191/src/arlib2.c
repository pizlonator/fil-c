/* Functions to handle creation of Linux archives.
   Copyright (C) 2007 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2007.

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

#include <limits.h>
#include <string.h>

#include "arlib.h"


/* Add long file name FILENAME of length FILENAMELEN to the symbol table
   SYMTAB.  Return the offset into the long file name table.  */
long int
arlib_add_long_name (const char *filename, size_t filenamelen)
{
  size_t size = obstack_object_size (&symtab.longnamesob);

  obstack_grow (&symtab.longnamesob, filename, filenamelen);
  obstack_grow (&symtab.longnamesob, "/\n", 2);

  return size - sizeof (struct ar_hdr);
}
