/* Generic definitions for libc main startup.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

#ifndef _LIBC_START_H
#define _LIBC_START_H

#ifndef SHARED
/* Static startup runs ARCH_SETUP_TLS (and writes the stack-protector
   canary) before ARCH_SETUP_IREL, so IFUNC resolvers can read TLS and
   stack-protector instrumented resolvers do not fault.  */
#define ARCH_SETUP_TLS()  __libc_setup_tls ()
#define ARCH_SETUP_IREL() apply_irel ()
#endif /* ! SHARED  */

#endif /* _LIBC_START_H  */
