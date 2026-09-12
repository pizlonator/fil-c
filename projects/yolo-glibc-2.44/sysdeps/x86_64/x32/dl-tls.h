/* Thread-local storage handling in the ELF dynamic linker.  x86-64 version.
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

#ifndef _X86_64_DL_TLS_H
#define _X86_64_DL_TLS_H

#include <stdint.h>

/* Type used for the representation of TLS information in the GOT.  */
typedef struct dl_tls_index
{
  uint64_t ti_module;
  uint64_t ti_offset;
} tls_index;


extern void *__tls_get_addr (tls_index *ti);

/* Dynamic thread vector pointers point to the start of each
   TLS block.  */
#define TLS_DTV_OFFSET 0

/* Static TLS offsets are relative to the unadjusted thread pointer.  */
#define TLS_TP_OFFSET 0

#endif /* _X86_64_DL_TLS_H */
