/* x86-64 specific core note handling.
   Copyright (C) 2005, 2007, 2008 Red Hat, Inc.
   Copyright (C) H.J. Lu <hjl.tools@gmail.com>, 2015.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <elf.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>

#ifndef BITS
# define BITS 		64
# define BACKEND	x86_64_
#else
# define BITS 		32
# define BACKEND	x32_
#endif
#include "libebl_CPU.h"


static const Ebl_Register_Location prstatus_regs[] =
  {
#define GR(at, n, dwreg)						\
    { .offset = at * 8, .regno = dwreg, .count = n, .bits = 64 }
#define SR(at, n, dwreg)						\
    { .offset = at * 8, .regno = dwreg, .count = n, .bits = 16, .pad = 6 }

    GR (0, 1, 15),		/* %r15 */
    GR (1, 1, 14),		/* %r14 */
    GR (2, 1, 13),		/* %r13 */
    GR (3, 1, 12),		/* %r12 */
    GR (4, 1, 6),		/* %rbp */
    GR (5, 1, 3),		/* %rbx */
    GR (6, 1, 11),		/* %r11 */
    GR (7, 1, 10),		/* %r10 */
    GR (8, 1, 9),		/* %r9 */
    GR (9, 1, 8),		/* %r8 */
    GR (10,1, 0),		/* %rax */
    GR (11,1, 2),		/* %rcx */
    GR (12,1, 1),		/* %rdx */
    GR (13,2, 4),		/* %rsi-%rdi */
    /*  15,1,			    orig_rax */
    GR (16,1, 16),		/* %rip */
    SR (17,1, 51),		/* %cs */
    GR (18,1, 49),		/* %rFLAGS */
    GR (19,1, 7),		/* %rsp */
    SR (20,1, 52),		/* %ss */
    GR (21,2, 58),		/* %fs.base-%gs.base */
    SR (23,1, 53),		/* %ds */
    SR (24,1, 50),		/* %es */
    SR (25,2, 54),		/* %fs-%gs */

#undef	GR
#undef	SR
  };
#define PRSTATUS_REGS_SIZE	(27 * 8)

#if BITS == 32
# define ULONG			uint32_t
# define ALIGN_ULONG		4
# define TYPE_ULONG		ELF_T_WORD
# define PRPSINFO_UID_T		uint16_t
# define ALIGN_PRPSINFO_UID_T	2
# define TYPE_PRPSINFO_UID_T	ELF_T_HALF
# define PRPSINFO_GID_T		uint16_t
# define ALIGN_PRPSINFO_GID_T	2
# define TYPE_PRPSINFO_GID_T	ELF_T_HALF
#else
# define ULONG			uint64_t
# define ALIGN_ULONG		8
# define TYPE_ULONG		ELF_T_XWORD
# define PRPSINFO_UID_T		uint32_t
# define ALIGN_PRPSINFO_UID_T	4
# define TYPE_PRPSINFO_UID_T	TYPE_UID_T
# define PRPSINFO_GID_T		uint32_t
# define ALIGN_PRPSINFO_GID_T	4
# define TYPE_PRPSINFO_GID_T	TYPE_GID_T
#endif
#define PR_REG			uint64_t
#define ALIGN_PR_REG		8
#define PID_T			int32_t
#define	UID_T			uint32_t
#define	GID_T			uint32_t
#define ALIGN_PID_T		4
#define ALIGN_UID_T		4
#define ALIGN_GID_T		4
#define TYPE_PID_T		ELF_T_SWORD
#define TYPE_UID_T		ELF_T_SWORD
#define TYPE_GID_T		ELF_T_SWORD

#define PRSTATUS_REGSET_ITEMS						      \
  {									      \
    .name = "orig_rax", .type = ELF_T_SXWORD, .format = 'd',		      \
    .offset = offsetof (struct EBLHOOK(prstatus), pr_reg) + (8 * 15),	      \
    .group = "register"	       			  	       	 	      \
  }

static const Ebl_Register_Location fpregset_regs[] =
  {
    { .offset = 0, .regno = 65, .count = 2, .bits = 16 }, /* fcw-fsw */
    { .offset = 24, .regno = 64, .count = 1, .bits = 32 }, /* mxcsr */
    { .offset = 32, .regno = 33, .count = 8, .bits = 80, .pad = 6 }, /* stN */
    { .offset = 32 + 128, .regno = 17, .count = 16, .bits = 128 }, /* xmm */
  };
#define FPREGSET_SIZE	512

#define	EXTRA_NOTES	EXTRA_NOTES_IOPERM

#include "x86_corenote.c"
#include "linux-core-note.c"
