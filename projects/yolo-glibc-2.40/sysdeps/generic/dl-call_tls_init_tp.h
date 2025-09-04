/* Invoke TLS_INIT_TP and __tls_init_tp with error handling.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <startup.h>
#include <tls.h>

static inline void
_startup_fatal_tls_error (void)
{
  _startup_fatal ("Fatal glibc error: Cannot allocate TLS block\n");
}

static inline void
call_tls_init_tp (void *addr)
{
  if (!TLS_INIT_TP (addr))
    _startup_fatal_tls_error ();
  __tls_init_tp ();
}
