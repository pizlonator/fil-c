/* Assembler macro definitions.  OpenRISC version.
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

#include <sysdeps/generic/sysdep.h>
#include <features.h>

#if defined __ASSEMBLER__ || defined REQUEST_ASSEMBLER_MACROS

/* Make use of .size directive.  */
#define ASM_SIZE_DIRECTIVE(name) .size name,.-name;

/* Define an entry point visible from C.  */
#define ENTRY(name)                                                           \
  .globl C_SYMBOL_NAME(name);                                                 \
  .type C_SYMBOL_NAME(name),@function;                                        \
  .align 4;                                                                   \
  C_LABEL(name)                                                               \
  cfi_startproc;                                                              \
  CALL_MCOUNT

#undef  END
#define END(name)                                                             \
  cfi_endproc;                                                                \
  ASM_SIZE_DIRECTIVE(name)

/* Since C identifiers are not normally prefixed with an underscore
   on this system, the asm identifier `syscall_error' intrudes on the
   C name space.  Make sure we use an innocuous name.  */
#define syscall_error   __syscall_error

/* If compiled for profiling, call `mcount' at the start of each function.  */
#ifdef  PROF
# ifdef __PIC__
#  define CALL_MCOUNT						\
	l.addi	r1, r1, -8;					\
	l.sw	0(r1), r9;					\
	l.sw	4(r1), r3;					\
	l.ori	r3, r9, 0;					\
	l.j	plt(_mcount);					\
	 l.nop;							\
	l.lwz	r9, 0(r1);					\
	l.lwz	r3, 4(r1);					\
	l.addi	r1, r1, 8;
# else
#  define CALL_MCOUNT						\
	l.addi	r1, r1, -8;					\
	l.sw	0(r1), r9;					\
	l.sw	4(r1), r3;					\
	l.ori	r3, r9, 0;					\
	l.movhi r15, hi(_mcount);				\
	l.ori	r15, r15, lo(_mcount);				\
	l.jr	r15;						\
	 l.nop;							\
	l.lwz	r9, 0(r1);					\
	l.lwz	r3, 4(r1);					\
	l.addi	r1, r1, 8;
# endif
#else
# define CALL_MCOUNT             /* Do nothing.  */
#endif

/* Local label name for asm code.  */
#define L(name)         .L##name

#endif /* __ASSEMBLER__ */
