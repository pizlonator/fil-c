/* Get loaded objects program headers.
   Copyright (C) 2001-2024 Free Software Foundation, Inc.
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

/* glibc's stock __dl_iterate_phdr walks the internal link_map and hands the
   callback pointers straight into the host ELF images (dlpi_name, dlpi_phdr),
   which user (Fil-C) code cannot legally form a pointer to, so Fil-C traps.
   Route the call through zsys_dl_iterate_phdr instead: the runtime enumerates
   the loaded objects with the host dl_iterate_phdr and rebuilds each struct
   dl_phdr_info in capability-safe Fil-C objects before invoking the callback.
   This is the glibc twin of the usermusl routing, so dl_iterate_phdr works
   under both libcs rather than only musl.  */
int
__dl_iterate_phdr (int (*callback) (struct dl_phdr_info *info,
				    size_t size, void *data), void *data)
{
  return zsys_dl_iterate_phdr ((void *) callback, data);
}
hidden_def (__dl_iterate_phdr)

weak_alias (__dl_iterate_phdr, dl_iterate_phdr);
