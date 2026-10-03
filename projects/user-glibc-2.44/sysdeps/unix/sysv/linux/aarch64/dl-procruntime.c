/* Data for processor runtime information.  AArch64 version.
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

#ifndef PROCINFO_CLASS
# define PROCINFO_CLASS
#endif

#if !IS_IN (ldconfig)
/* Fil-C: dl-support.c (which includes this file) is built into libc.so as
   well as libc.a in the pizlonated build, so the !PROCINFO_DECL + SHARED
   "array element" form cannot be used.  Use plain declarations, matching
   sysdeps/x86_64/dl-procruntime.c.  */
PROCINFO_CLASS unsigned long _dl_aarch64_gcs
# ifndef PROCINFO_DECL
= 0
# endif
;
#endif
