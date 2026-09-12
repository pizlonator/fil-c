/* Fil-C: route glibc's dl_find_object through the runtime.
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
#include <pizlonated_syscalls.h>

/* glibc's stock _dl_find_object walks the internal find_object tables and hands
   back host pointers (dlfo_map_start, dlfo_link_map) that user (Fil-C) code
   cannot legally form, so Fil-C traps.  Route the call through
   zsys_dl_find_object instead: the runtime resolves the object from the loaded
   program headers and fills a capability-safe struct dl_find_object.  This is
   the glibc twin of the usermusl routing, so dl_find_object works under both
   libcs rather than only musl.  */
int
__dl_find_object (void *pc, struct dl_find_object *result)
{
  return zsys_dl_find_object (pc, result);
}
hidden_def (__dl_find_object)

weak_alias (__dl_find_object, _dl_find_object)
