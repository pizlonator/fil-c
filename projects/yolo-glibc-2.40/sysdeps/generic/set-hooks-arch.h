/* Machine-dependent macros for using symbol sets for running lists of
   functions. Generic/stub version.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#ifndef _SET_HOOKS_ARCH_H
#define _SET_HOOKS_ARCH_H

/* Define SET_RELHOOK to a variant of text_set_element that records a relative
   offset rather than an absolute address. See sysdeps/mach/hurd/x86/set-hooks-arch.h
   for an example.

#define SET_RELHOOK(NAME, HOOK) ...

 */

#endif /* set_hooks_arch.h */
