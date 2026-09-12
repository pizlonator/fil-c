/* Thread-local storage descriptor handling in the ELF dynamic linker.
   LoongArch version.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.

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

#ifndef _DL_TLSDESC_H
#define _DL_TLSDESC_H

#include <dl-tls.h>

/* Type used to represent a TLS descriptor in the GOT.  */
struct tlsdesc
{
  ptrdiff_t (*entry) (struct tlsdesc *);
  void *arg;
};

/* Type used as the argument in a TLS descriptor for a symbol that
   needs dynamic TLS offsets.  */
struct tlsdesc_dynamic_arg
{
  tls_index tlsinfo;
  size_t gen_count;
};

extern ptrdiff_t attribute_hidden _dl_tlsdesc_return (struct tlsdesc *);
extern ptrdiff_t attribute_hidden _dl_tlsdesc_undefweak (struct tlsdesc *);

#ifdef SHARED
extern void *_dl_make_tlsdesc_dynamic (struct link_map *, size_t);
#ifndef __loongarch_soft_float
extern ptrdiff_t attribute_hidden _dl_tlsdesc_dynamic_lasx (struct tlsdesc *);
extern ptrdiff_t attribute_hidden _dl_tlsdesc_dynamic_lsx (struct tlsdesc *);
#endif
extern ptrdiff_t attribute_hidden _dl_tlsdesc_dynamic (struct tlsdesc *);
#endif

#endif
