/* Computation of TLS block size and its alignment.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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

/* Compute the size of the TLS block for memory allocation.  */
static size_t
_dl_tls_block_size_with_pre (void)
{
  size_t size = GLRO (dl_tls_static_size);

#if TLS_DTV_AT_TP
  /* Memory layout is:
     [ TLS_PRE_TCB_SIZE ] [ TLS_TCB_SIZE ] [ TLS blocks ]
                          ^ This should be returned.  */
  size += TLS_PRE_TCB_SIZE;
#endif
  return size;
}

/* SIZE must be the value returned by _dl_tls_block_size_with_pre.
   ALLOCATED must point to at least SIZE + GLRO (dl_tls_static_align)
   bytes.  */
static void *
_dl_tls_block_align (size_t size, void *allocated)
{
  void *result;
  size_t alignment = GLRO (dl_tls_static_align);

#if TLS_TCB_AT_TP
  /* The TCB follows the TLS blocks, which determine the alignment.
     (TCB alignment requirements have been taken into account when
     calculating GLRO (dl_tls_static_align).)  */
  void *aligned = (void *) roundup ((uintptr_t) allocated, alignment);
  result = aligned + size - TLS_TCB_SIZE;

  /* Clear the TCB data structure.  We can't ask the caller (i.e.
     libpthread) to do it, because we will initialize the DTV et al.  */
  memset (result, '\0', TLS_TCB_SIZE);
#elif TLS_DTV_AT_TP
  /* Pre-TCB and TCB come before the TLS blocks.  The layout computed
     in _dl_determine_tlsoffset assumes that the TCB is aligned to the
     TLS block alignment, and not just the TLS blocks after it.  This
     can leave an unused alignment gap between the TCB and the TLS
     blocks.  */
  result = (void *) roundup
    (sizeof (void *) + TLS_PRE_TCB_SIZE + (uintptr_t) allocated,
     alignment);

  /* Clear the TCB data structure and TLS_PRE_TCB_SIZE bytes before
     it.  We can't ask the caller (i.e. libpthread) to do it, because
     we will initialize the DTV et al.  */
  memset (result - TLS_PRE_TCB_SIZE, '\0', TLS_PRE_TCB_SIZE + TLS_TCB_SIZE);
#endif

  return result;
}
