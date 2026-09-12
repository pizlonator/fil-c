/* _dl_executable_postprocess.  Generic version.
   Copyright (C) 2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

static inline void
_dl_executable_postprocess (struct link_map *main_map,
			    const ElfW(Phdr) *phdr, ElfW(Word) phnum)
{
#ifdef SHARED
  /* Process program headers again, but scan them backwards since
     PT_GNU_PROPERTY is close to the end of program headers.   */
  for (const ElfW(Phdr) *ph = &phdr[phnum]; ph != phdr; --ph)
    if (ph[-1].p_type == PT_GNU_PROPERTY)
      {
	_dl_process_pt_gnu_property (main_map, -1, &ph[-1]);
	break;
      }
#endif
}
