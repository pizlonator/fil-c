/* AArch64 definitions for libc main startup.
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

#ifndef _LIBC_START_H
#define _LIBC_START_H

/* Fil-C: the upstream AArch64 version of this file has a !SHARED-only
   block that calls __libc_setup_tls plus the GCS (guarded control stack)
   setup code (_dl_process_pt_gnu_property, _rtld_main_check and raw prctl
   syscalls to enable/lock GCS).  Those functions and the GCS prctls are
   rtld-only or otherwise unsuitable for the pizlonated libc, so this now
   mirrors sysdeps/x86_64/libc-start.h: the TLS setup is just
   __libc_setup_tls (from csu/libc-tls.c, which is Fil-C-ified), and there
   is no IREL / GCS setup.  */

# define ARCH_SETUP_IREL() apply_irel ()
# define ARCH_SETUP_TLS() __libc_setup_tls ()

#endif /* _LIBC_START_H  */
