/* Test bug in dwfl_report_segment() coalescing.
   Copyright (C) 2019 Facebook
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
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include ELFUTILS_HEADER(dwfl)


static const Dwfl_Callbacks offline_callbacks =
  {
    .find_debuginfo = INTUSE(dwfl_standard_find_debuginfo),
    .section_address = INTUSE(dwfl_offline_section_address),
  };


int
main (void)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  Dwfl *dwfl = dwfl_begin (&offline_callbacks);
  assert (dwfl != NULL);

  GElf_Phdr phdr1 =
    {
      .p_type = PT_LOAD,
      .p_flags = PF_R,
      .p_offset = 0xf00,
      .p_vaddr = 0xf00,
      .p_filesz = 0x100,
      .p_memsz = 0x100,
      .p_align = 4,
    };

  int ndx = dwfl_report_segment (dwfl, 1, &phdr1, 0, dwfl);
  assert(ndx == 1);

  ndx = dwfl_addrsegment (dwfl, 0xf00, NULL);
  assert(ndx == 1);

  GElf_Phdr phdr2 =
    {
      .p_type = PT_LOAD,
      .p_flags = PF_R | PF_W,
      .p_offset = 0x1000,
      .p_vaddr = 0x1000,
      .p_filesz = 0x100,
      .p_memsz = 0x100,
      .p_align = 4,
    };
  ndx = dwfl_report_segment (dwfl, 2, &phdr2, 0, dwfl);
  assert(ndx == 2);

  ndx = dwfl_addrsegment (dwfl, 0x1000, NULL);
  assert(ndx == 1 || ndx == 2);

  dwfl_end (dwfl);

  return 0;
}
