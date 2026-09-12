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

#include <dl-load.h>

extern ElfW (Addr) _dl_map_segment_align
  (const struct loadcmd *, ElfW (Addr)) attribute_hidden;

/* Return true only if the loadcmd C is THP eligible with THP page size
   THP_PAGESIZE, which means that it is read-only, its size >= THP page
   size, its offset and address of the loadcmd C are aligned to THP page
   size.  */

static inline bool
_dl_segment_thp_eligible (const struct loadcmd *c, size_t thp_pagesize)
{
  return ((c->prot & (PROT_WRITE | PROT_READ)) == PROT_READ
	  && (c->mapend - c->mapstart) >= thp_pagesize
	  && ((c->mapstart | c->mapoff) & (thp_pagesize - 1)) == 0);
}
