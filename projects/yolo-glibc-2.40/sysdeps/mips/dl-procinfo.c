/* Data for Mips version of processor capability information.
   Copyright (C) 2007-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

/* Note:
   When compiling elf/ldconfig.c, PROCINFO_CLASS is defined to static.
   This dl-procinfo.c is included in sysdeps/generic/ldsodefs.h.
   Afterwards, if not yet defined, PROCINFO_CLASS is defined to EXTERN
   just before dl-vdso-setup.c is included.  A "static" _dl_vdso_xyz
   function prototype would lead to gcc warnings/errors: defined but
   not used.  */
#undef PROCINFO_DECL
#undef PROCINFO_CLASS
