/* Declarations for dynamic linker interface. OpenRISC version.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _LINK_H
# error "Never include <bits/link.h> directly; use <link.h> instead."
#endif

/* Registers for entry into PLT.  */
typedef struct La_or1k_regs
{
  uint32_t lr_reg[6]; /* Args passed in r3-r8.  */
  uint32_t lr_sp;     /* Register r1.  */
  uint32_t lr_lr;     /* Register r9.  */
} La_or1k_regs;

/* Return values for calls from PLT.  */
typedef struct La_or1k_retval
{
  /* Up to two 32-bit registers used for a return value.  */
  uint32_t lrv_reg[2];
} La_or1k_retval;

__BEGIN_DECLS

extern Elf32_Addr la_or1k_gnu_pltenter (Elf32_Sym *__sym, unsigned int __ndx,
					uintptr_t *__refcook,
					uintptr_t *__defcook,
					La_or1k_regs *__regs,
					unsigned int *__flags,
					const char *__symname,
					long int *__framesizep);
extern unsigned int la_or1k_gnu_pltexit (Elf32_Sym *__sym, unsigned int __ndx,
					 uintptr_t *__refcook,
					 uintptr_t *__defcook,
					 const La_or1k_regs *__inregs,
					 La_or1k_retval *__outregs,
					 const char *__symname);

__END_DECLS
