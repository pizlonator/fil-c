/* Explicit test compiling with system elf.h header plus libelf/gelf headers.

   Copyright (C) Mark J. Wielaard <mark@klomp.org>
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

#include <elf.h>
#include <stddef.h>
#include "../libelf/libelf.h"
#include "../libelf/gelf.h"

int
main (void)
{
  /* Trivial test, this is really a compile test anyway.  */
  if (elf_version (EV_CURRENT) == EV_NONE)
    return -1;

  /* This will obviously fail. It is just to check that gelf_getclass
     available (both at compile time and runtime).  */
  int cls = gelf_getclass (NULL);

  return cls == ELFCLASSNONE ? 0 : -1;
}
