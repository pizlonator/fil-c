/* ARM specific core note handling.
   Copyright (C) 2009, 2012 Red Hat, Inc.
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

#define BACKEND arm_
#include "libebl_CPU.h"


static const Ebl_Register_Location prstatus_regs[] =
  {
    { .offset = 0, .regno = 0, .count = 16, .bits = 32 },	/* r0..r15 */
    { .offset = 16 * 4, .regno = 128, .count = 1, .bits = 32 }, /* cpsr */
  };
#define PRSTATUS_REGS_SIZE	(18 * 4)

#define PRSTATUS_REGSET_ITEMS						      \
  {									      \
    .name = "orig_r0", .type = ELF_T_SWORD, .format = 'd',		      \
    .offset = offsetof (struct EBLHOOK(prstatus), pr_reg) + (4 * 17),	      \
    .group = "register"	       			  	       	 	      \
  }

static const Ebl_Register_Location fpregset_regs[] =
  {
    { .offset = 0, .regno = 96, .count = 8, .bits = 96 }, /* f0..f7 */
  };
#define FPREGSET_SIZE	116

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

#define ARM_VFPREGS_SIZE ( 32 * 8 /*fpregs*/ + 4 /*fpscr*/ )
static const Ebl_Register_Location vfp_regs[] =
  {
    { .offset = 0, .regno = 256, .count = 32, .bits = 64 }, /* fpregs */
  };

static const Ebl_Core_Item vfp_items[] =
  {
    {
      .name = "fpscr", .group = "register",
      .offset = 0,
      .type = ELF_T_WORD, .format = 'x',
    },
  };

#define	EXTRA_NOTES \
  EXTRA_REGSET_ITEMS (NT_ARM_VFP, ARM_VFPREGS_SIZE, vfp_regs, vfp_items)

#include "linux-core-note.c"
