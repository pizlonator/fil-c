/* SH specific core note handling.
   Copyright (C) 2010 Red Hat, Inc.
   This file is part of elfutils.
   Contributed Matt Fleming <matt@console-pimps.org>.

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

#define BACKEND		sh_
#include "libebl_CPU.h"

static const Ebl_Register_Location prstatus_regs[] =
  {
#define GR(at, n, dwreg)						\
    { .offset = at * 4, .regno = dwreg, .count = n, .bits = 32 }
    GR (0, 16, 0),		/* r0-r15 */
    GR (16, 1, 16),		/* pc */
    GR (17, 1, 17),		/* pr */
    GR (18, 1, 22),		/* sr */
    GR (19, 1, 18),		/* gbr */
    GR (20, 1, 20),		/* mach */
    GR (21, 1, 21),		/* macl */
    /*  22, 1,			   tra */
#undef GR
  };
#define PRSTATUS_REGS_SIZE	(23 * 4)

#define	ULONG			uint32_t
#define PID_T			int32_t
#define	UID_T			uint16_t
#define	GID_T			uint16_t
#define ALIGN_ULONG		4
#define ALIGN_PID_T		4
#define ALIGN_UID_T		2
#define ALIGN_GID_T		2
#define TYPE_ULONG		ELF_T_WORD
#define TYPE_PID_T		ELF_T_SWORD
#define TYPE_UID_T		ELF_T_HALF
#define TYPE_GID_T		ELF_T_HALF

#define PRSTATUS_REGSET_ITEMS						      \
  {									      \
    .name = "tra", .type = ELF_T_ADDR, .format = 'x',			      \
    .offset = offsetof (struct EBLHOOK(prstatus), pr_reg[22]),		      \
    .group = "register"	       			  	       	 	      \
  }

static const Ebl_Register_Location fpregset_regs[] =
  {
    { .offset = 0, .regno = 25, .count = 16, .bits = 32 }, /* fr0-fr15 */
    { .offset = 16, .regno = 87, .count = 16, .bits = 32 }, /* xf0-xf15 */
    { .offset = 32, .regno = 24, .count = 1, .bits = 32 }, /* fpscr */
    { .offset = 33, .regno = 23, .count = 1, .bits = 32 }  /* fpul */
  };
#define FPREGSET_SIZE		(50 * 4)

#include "linux-core-note.c"
