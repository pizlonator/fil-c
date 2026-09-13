/* Fil-C: route glibc's _dl_find_object through the runtime.
   Copyright (C) 2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <link.h>
#include <dlfcn.h>
#include <pizlonated_syscalls.h>

/* glibc's stock _dl_find_object walks the internal find_object tables and hands
   back host pointers (dlfo_map_start, dlfo_link_map) that user (Fil-C) code
   cannot legally form, so Fil-C traps.  Route the call through
   zsys_dl_find_object instead: the runtime resolves the object from the loaded
   program headers and fills a capability-safe struct dl_find_object.  This is
   the glibc twin of the usermusl routing, so dl_find_object works under both
   libcs rather than only musl.

   _dl_find_object is an rtld symbol -- declared with rtld_hidden_proto in
   <dlfcn.h> -- so it is defined directly and exported via rtld_hidden_def,
   mirroring the stock dl-find_object.c aliasing.  The libc hidden_def/__-
   prefixed idiom used for __dl_iterate_phdr does not apply here: _dl_find_object
   is not a libc-hidden __-name with a public weak alias, and emitting a
   .filc_alias onto the already-defined name collides ("New alias name
   _dl_find_object is already taken by a definition"), aborting FilPizlonator.  */
int
_dl_find_object (void *pc, struct dl_find_object *result)
{
  return zsys_dl_find_object (pc, result);
}
rtld_hidden_def (_dl_find_object)
