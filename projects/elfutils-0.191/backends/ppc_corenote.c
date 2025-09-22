/* PowerPC specific core note handling.
   Copyright (C) 2007, 2008 Red Hat, Inc.
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
# define BITS 		32
# define BACKEND	ppc_
#else
# define BITS 		64
# define BACKEND	ppc64_
#endif
#include "libebl_CPU.h"

static const Ebl_Register_Location prstatus_regs[] =
  {
#define GR(at, n, dwreg)						\
    { .offset = at * BITS/8, .regno = dwreg, .count = n, .bits = BITS }

    GR (0, 32, 0),		/* r0-r31 */
    /*	32, 1,			   nip */
    GR (33, 1, 66),		/* msr */
    /*	34, 1,			   orig_gpr3 */
    GR (35, 1, 109),		/* ctr */
    GR (36, 1, 108),		/* lr */
    GR (37, 1, 101),		/* xer */
    GR (38, 1, 64),		/* cr */
    GR (39, 1, 100),		/* mq */
    /*  40, 1,			   trap */
    GR (41, 1, 119),		/* dar */
    GR (42, 1, 118),		/* dsisr */

#undef	GR
  };
#define PRSTATUS_REGS_SIZE	(BITS / 8 * 48)

static const Ebl_Register_Location fpregset_regs[] =
  {
    { .offset = 0, .regno = 32, .count = 32, .bits = 64 }, /* f0-f31 */
    { .offset = 32 * 8 + 4, .regno = 65, .count = 1, .bits = 32 } /* fpscr */
  };
#define FPREGSET_SIZE		(33 * 8)

static const Ebl_Register_Location altivec_regs[] =
  {
    /* vr0-vr31 */
    { .offset = 0, .regno = 1124, .count = 32, .bits = 128 },
    /* vscr XXX 67 is an unofficial assignment */
    { .offset = 32 * 16, .regno = 67, .count = 1, .bits = 32, .pad = 12 },
    /* vrsave */
    { .offset = 33 * 16, .regno = 356, .count = 1, .bits = 32, .pad = 12 }
  };

static const Ebl_Register_Location spe_regs[] =
  {
    /* evr0-evr31
    { .offset = 0, .regno = ???, .count = 32, .bits = 32 },
     * acc *
    { .offset = 32 * 4, .regno = ???, .count = 1, .bits = 64 }, */
    /* spefscr */
    { .offset = 34 * 4, .regno = 612, .count = 1, .bits = 32 }
  };

static const Ebl_Register_Location tm_spr_regs[] =
  {
    /* tfhar */
    { .offset = 0, .regno = 114, .count = 1, .bits = 64 },
    /* texasr */
    { .offset = 8, .regno = 116, .count = 1, .bits = 64 },
    /* tfiar */
    { .offset = 16, .regno = 115, .count = 1, .bits = 64 }
  };

#define EXTRA_NOTES \
  EXTRA_REGSET (NT_PPC_VMX, 34 * 16, altivec_regs) \
  EXTRA_REGSET (NT_PPC_SPE, 35 * 4, spe_regs) \
  EXTRA_REGSET (NT_PPC_TM_SPR, 3 * 8, tm_spr_regs)

#if BITS == 32
# define ULONG			uint32_t
# define ALIGN_ULONG		4
# define TYPE_ULONG		ELF_T_WORD
# define TYPE_LONG		ELF_T_SWORD
#else
# define ULONG			uint64_t
# define ALIGN_ULONG		8
# define TYPE_ULONG		ELF_T_XWORD
# define TYPE_LONG		ELF_T_SXWORD
#endif
#define PID_T			int32_t
#define	UID_T			uint32_t
#define	GID_T			uint32_t
#define ALIGN_PID_T		4
#define ALIGN_UID_T		4
#define ALIGN_GID_T		4
#define TYPE_PID_T		ELF_T_SWORD
#define TYPE_UID_T		ELF_T_WORD
#define TYPE_GID_T		ELF_T_WORD

#define PRSTATUS_REGSET_ITEMS						      \
  {									      \
    .name = "nip", .type = ELF_T_ADDR, .format = 'x',			      \
    .offset = offsetof (struct EBLHOOK(prstatus), pr_reg[32]),		      \
    .group = "register", .pc_register = true				      \
  },								      	      \
  {									      \
    .name = "orig_gpr3", .type = TYPE_LONG, .format = 'd',		      \
    .offset = offsetof (struct EBLHOOK(prstatus), pr_reg[34]),		      \
    .group = "register"	       			  	       	 	      \
  }

#include "linux-core-note.c"
