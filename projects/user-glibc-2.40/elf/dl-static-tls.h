/* Inline functions for dynamic linking.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#ifndef _DL_STATIC_TLS_H
#define _DL_STATIC_TLS_H

/* This macro is used as a callback from elf_machine_rel{a,} when a
   static TLS reloc is about to be performed.  Since (in dl-load.c) we
   permit dynamic loading of objects that might use such relocs, we
   have to check whether each use is actually doable.  If the object
   whose TLS segment the reference resolves to was allocated space in
   the static TLS block at startup, then it's ok.  Otherwise, we make
   an attempt to allocate it in surplus space on the fly.  If that
   can't be done, we fall back to the error that DF_STATIC_TLS is
   intended to produce.  */
#define HAVE_STATIC_TLS(map, sym_map)					\
    (__builtin_expect ((sym_map)->l_tls_offset != NO_TLS_OFFSET		\
		       && ((sym_map)->l_tls_offset			\
			   != FORCED_DYNAMIC_TLS_OFFSET), 1))

#define CHECK_STATIC_TLS(map, sym_map)					\
    do {								\
      if (!HAVE_STATIC_TLS (map, sym_map))				\
	_dl_allocate_static_tls (sym_map);				\
    } while (0)

#define TRY_STATIC_TLS(map, sym_map)					\
    (__builtin_expect ((sym_map)->l_tls_offset				\
		       != FORCED_DYNAMIC_TLS_OFFSET, 1)			\
     && (__builtin_expect ((sym_map)->l_tls_offset != NO_TLS_OFFSET, 1)	\
	 || _dl_try_allocate_static_tls (sym_map, true) == 0))

int _dl_try_allocate_static_tls (struct link_map *map, bool optional)
  attribute_hidden;

#endif
