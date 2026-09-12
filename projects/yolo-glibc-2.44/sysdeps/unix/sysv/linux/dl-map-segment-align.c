/* _dl_map_segment_align.  Linux version.
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

#include <ldsodefs.h>
#include <dl-map-segment-align.h>

/* Return the alignment of the PT_LOAD segment for THP.  P_ALIGN_MAX is
   the maximum p_align value in the PT_LOAD segment.  */

ElfW (Addr)
_dl_map_segment_align (const struct loadcmd *c, ElfW (Addr) p_align_max)
{
  size_t thp_pagesize = GL(dl_elf_thp_pagesize);

  if (GL(dl_elf_thp_control) != dl_elf_thp_control_enabled
      || p_align_max >= thp_pagesize)
    return p_align_max;

  /* Return true if the segment is THP eligible.  It helps improve THP
     eligibility and reduces TLB pressure.  */
  if (_dl_segment_thp_eligible (c, thp_pagesize))
    return thp_pagesize;

  return p_align_max;
}
