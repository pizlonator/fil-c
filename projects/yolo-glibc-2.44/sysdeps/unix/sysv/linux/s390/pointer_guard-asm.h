/* Pointer obfuscation implementation, assembly version.  s390x version.
   Copyright (C) 2005-2026 Free Software Foundation, Inc.
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

#ifndef POINTER_GUARD_ASM_H
#define POINTER_GUARD_ASM_H

#ifdef __ASSEMBLER__
# if IS_IN (rtld) || !defined SHARED
#  define PTR_GUARD_LOAD(tmpreg)					\
	larl	tmpreg,__pointer_chk_guard_local;			\
	lg	tmpreg,0(tmpreg)
# else
#  define PTR_GUARD_LOAD(tmpreg)					\
	larl	tmpreg,__pointer_chk_guard@GOTENT;			\
	lg	tmpreg,0(tmpreg);					\
	lg	tmpreg,0(tmpreg)
# endif
# define PTR_MANGLE(reg, tmpreg) \
	PTR_GUARD_LOAD (tmpreg);					\
	xgr	reg,tmpreg;						\
	rllg	reg,reg,17
# define PTR_MANGLE2(reg, tmpreg) \
	xgr	reg,tmpreg;						\
	rllg	reg,reg,17
# define PTR_DEMANGLE(reg, tmpreg) \
	PTR_GUARD_LOAD (tmpreg);					\
	rllg	reg,reg,47;						\
	xgr	reg,tmpreg
# define PTR_DEMANGLE2(reg, tmpreg) \
	rllg	reg,reg,47;						\
	xgr	reg,tmpreg
#endif

#endif /* POINTER_GUARD_ASM_H */
