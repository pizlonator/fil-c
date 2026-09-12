/* extra tls block utils for the dynamic linker.  Linux version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#ifndef _DL_EXTRA_TLS_H
#define _DL_EXTRA_TLS_H 1

#include <stddef.h>
#include <sys/rseq.h>
#include <rseq-internal.h>
#include <elf/dl-tunables.h>

/* Returns the size of the extra TLS block, it must always be a multiple of the
   alignment.  */
static inline size_t
_dl_extra_tls_get_size (void)
{
  bool do_rseq = TUNABLE_GET_FULL (glibc, pthread, rseq, int, NULL);
  if (do_rseq)
    {
      /* Make sure the rseq area size is at least the minimum ABI size and a
         multiple of the requested aligment.  */
      return roundup (MAX (_rseq_size, RSEQ_AREA_SIZE_INITIAL), _rseq_align);
    }

  /* Even when disabled by tunable, an rseq area will be allocated to allow
     application code to test the registration status with 'rseq->cpu_id >= 0'.
     Default to the rseq ABI minimum size, this will ensure we don't use more
     TLS than necessary.  */
  return RSEQ_AREA_SIZE_INITIAL;
}

/* Returns the alignment requirements of the extra TLS block.  */
static inline size_t
_dl_extra_tls_get_align (void)
{
  bool do_rseq = TUNABLE_GET_FULL (glibc, pthread, rseq, int, NULL);
  if (do_rseq)
    {
      return _rseq_align;
    }

  /* Even when disabled by tunable, an rseq area will be allocated to allow
     application code to test the registration status with 'rseq->cpu_id >= 0'.
     Default to the rseq ABI minimum alignment, this will ensure we don't use
     more TLS than necessary.  */
  return RSEQ_MIN_ALIGN;
}

/* Record the offset of the extra TLS block from the thread pointer.  */
static inline void
_dl_extra_tls_set_offset (ptrdiff_t tls_offset)
{
    _rseq_offset = tls_offset;
}

#endif
