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
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include ELFUTILS_HEADER(dwfl)
#include ELFUTILS_HEADER(elf)

static const Dwfl_Callbacks cb =
{
  NULL,
  NULL,
  NULL,
  NULL,
};

int
main (int argc, char **argv)
{
  assert (argc == 2);

  Dwfl *dwfl = dwfl_begin (&cb);

  int fd = open (argv[1], O_RDONLY);
  assert (fd != -1);

  Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
  (void) dwfl_core_file_report (dwfl, elf, argv[0]);

  /* testcore-noncontig contains a shared library mapped between 
     non-contiguous segments of another shared library:

     [...]
     7f14e458c000-7f14e45ae000 00000000 139264      /usr/lib64/ld-2.17.so             (1)
     7f14e4795000-7f14e4798000 00000000 12288       /usr/lib64/firefox/liblgpllibs.so (2)
     7f14e4798000-7f14e479d000 00003000 20480       /usr/lib64/firefox/liblgpllibs.so
     7f14e479d000-7f14e479f000 00008000 8192        /usr/lib64/firefox/liblgpllibs.so
     7f14e479f000-7f14e47a0000 00009000 4096        /usr/lib64/firefox/liblgpllibs.so
     7f14e47a0000-7f14e47a1000 0000a000 4096        /usr/lib64/firefox/liblgpllibs.so (3)
     7f14e47ad000-7f14e47ae000 00021000 4096        /usr/lib64/ld-2.17.so             (4)
     7f14e47ae000-7f14e47af000 00022000 4096        /usr/lib64/ld-2.17.so  */

  /* First segment of the non-contiguous module (1).  */
  int seg = dwfl_addrsegment (dwfl, 0x7f14e458c000, NULL);
  assert (seg == 32);

  /* First segment of the module within the non-contiguous module's address
     range (2).  */
  seg = dwfl_addrsegment (dwfl, 0x7f14e4795000, NULL);
  assert (seg == 33);

  /* Last segment of the module within the non-contiguous module's
     address range (3).  */
  seg = dwfl_addrsegment (dwfl, 0x7f14e47a0000, NULL);
  assert (seg == 37);

  /* First segment of non-contiguous module following its address space
     gap (4).  */
  seg = dwfl_addrsegment (dwfl, 0x7f14e47ad000, NULL);
  assert (seg == 40);

  dwfl_end (dwfl);
  elf_end (elf);

  return 0;
}
