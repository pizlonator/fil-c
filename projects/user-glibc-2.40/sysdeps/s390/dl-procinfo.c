/* Data for s390 version of processor capability information.
   Copyright (C) 2006-2024 Free Software Foundation, Inc.
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

/* The hwcap and platform strings are now in
   sysdeps/s390/dl-procinfo-s390.c.  */

/* Needed by sysdeps/unix/sysv/linux/dl-vdso-setup.c (as included from
   sysdeps/generic/ldsodefs.h).  */

/* All the #ifdefs in the definitions are quite irritating but
   necessary if we want to avoid duplicating the information.  There
   are three different modes:

   - PROCINFO_DECL is defined.  This means we are only interested in
     declarations.

   - PROCINFO_DECL is not defined:

     + if SHARED is defined the file is included in an array
       initializer.  The .element = { ... } syntax is needed.

     + if SHARED is not defined a normal array initialization is
       needed.
  */

#ifndef PROCINFO_CLASS
# define PROCINFO_CLASS
#endif

#if !IS_IN (ldconfig)
# if !defined PROCINFO_DECL && defined SHARED
  ._dl_s390_cpu_features
# else
PROCINFO_CLASS struct cpu_features _dl_s390_cpu_features
# endif
# ifndef PROCINFO_DECL
= { }
# endif
# if !defined SHARED || defined PROCINFO_DECL
;
# else
,
# endif
#endif

#undef PROCINFO_DECL
#undef PROCINFO_CLASS
