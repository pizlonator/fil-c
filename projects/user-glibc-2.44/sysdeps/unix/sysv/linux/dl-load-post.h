/* _dl_postprocess_loadcmd_extra.  Linux version.
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

#include "dl-thp-madvise.h"

static bool _dl_segment_thp_eligible (const struct loadcmd *, size_t);

/* After L has been mapped in, call madvise with MADV_HUGEPAGE for THP
   madvise mode if L is THP eligible.  */

static inline void
_dl_postprocess_loadcmd_extra (struct link_map *l, const struct loadcmd *c)
{
  if (GL(dl_thp_mode) == thp_mode_madvise
      && _dl_segment_thp_eligible (c, GL(dl_elf_thp_pagesize)))
    {
      int ret = _dl_thp_madvise ((void *) (l->l_addr + c->mapstart),
				 c->mapend - c->mapstart);
      if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))
	_dl_debug_printf ("\
  madvise (0x%0*lx, 0x%0*lx, MADV_HUGEPAGE) returns %d\n",
  (int) sizeof (void *) * 2,
  (unsigned long int) (l->l_addr + c->mapstart),
  (int) sizeof (void *) * 2,
  (unsigned long int) (c->mapend - c->mapstart),
  ret);
    }
}
