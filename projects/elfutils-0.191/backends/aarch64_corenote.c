/* AArch64 specific core note handling.
   Copyright (C) 2013 Red Hat, Inc.
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

#define BACKEND aarch64_
#include "libebl_CPU.h"

#define	ULONG			uint64_t
#define PID_T			int32_t
#define	UID_T			uint32_t
#define	GID_T			uint32_t
#define ALIGN_ULONG		8
#define ALIGN_PID_T		4
#define ALIGN_UID_T		4
#define ALIGN_GID_T		4
#define TYPE_ULONG		ELF_T_XWORD
#define TYPE_PID_T		ELF_T_SWORD
#define TYPE_UID_T		ELF_T_WORD
#define TYPE_GID_T		ELF_T_WORD

#define PRSTATUS_REGS_SIZE	(34 * 8)

static const Ebl_Register_Location prstatus_regs[] =
  {
    { .offset = 0, .regno = 0, .count = 32, .bits = 64 }, /* x0..x30, sp */
  };

#define PRSTATUS_REGSET_ITEMS						\
  {									\
    .name = "pc", .type = ELF_T_XWORD, .format = 'x',			\
    .offset = (offsetof (struct EBLHOOK(prstatus), pr_reg)		\
	       + PRSTATUS_REGS_SIZE - 16),				\
    .group = "register",						\
    .pc_register = true							\
  },									\
  {									\
    .name = "pstate", .type = ELF_T_XWORD, .format = 'x',		\
    .offset = (offsetof (struct EBLHOOK(prstatus), pr_reg)		\
	       + PRSTATUS_REGS_SIZE - 8),				\
    .group = "register"							\
  }

static const Ebl_Register_Location aarch64_fpregset_regs[] =
  {
    { .offset = 0, .regno = 64, .count = 32, .bits = 128 }, /* v0..v31 */
  };

static const Ebl_Core_Item aarch64_fpregset_items[] =
  {
    {
      .name = "fpsr", .type = ELF_T_WORD, .format = 'x',
      .offset = 512, .group = "register"
    },
    {
      .name = "fpcr", .type = ELF_T_WORD, .format = 'x',
      .offset = 516, .group = "register"
    }
  };

static const Ebl_Core_Item aarch64_tls_items[] =
  {
    {
      .name = "tls", .type = ELF_T_XWORD, .format = 'x',
      .offset = 0, .group = "register"
    }
  };

static const Ebl_Core_Item aarch64_syscall_items [] =
  {
    {
      .name = "syscall", .type = ELF_T_WORD, .format = 'x',
      .offset = 0, .group = "register"
    }
  };

#define AARCH64_HWBP_REG(KIND, N)					\
    {									\
      .name = "DBG" KIND "VR" #N "_EL1", .type = ELF_T_XWORD, .format = 'x', \
      .offset = 8 + N * 16, .group = "register"				\
    },									\
    {									\
      .name = "DBG" KIND "CR" #N "_EL1", .type = ELF_T_WORD, .format = 'x', \
      .offset = 16 + N * 16, .group = "register"			\
    }

#define AARCH64_BP_WP_GROUP(KIND, NAME)					\
  static const Ebl_Core_Item NAME[] =					\
    {									\
      {									\
	.name = "dbg_info", .type = ELF_T_WORD, .format = 'x',		\
	.offset = 0, .group = "control"					\
      },								\
      /* N.B.: 4 bytes of padding here.  */				\
									\
      AARCH64_HWBP_REG(KIND, 0),					\
      AARCH64_HWBP_REG(KIND, 1),					\
      AARCH64_HWBP_REG(KIND, 2),					\
      AARCH64_HWBP_REG(KIND, 3),					\
      AARCH64_HWBP_REG(KIND, 4),					\
      AARCH64_HWBP_REG(KIND, 5),					\
      AARCH64_HWBP_REG(KIND, 6),					\
      AARCH64_HWBP_REG(KIND, 7),					\
      AARCH64_HWBP_REG(KIND, 8),					\
      AARCH64_HWBP_REG(KIND, 9),					\
      AARCH64_HWBP_REG(KIND, 10),					\
      AARCH64_HWBP_REG(KIND, 11),					\
      AARCH64_HWBP_REG(KIND, 12),					\
      AARCH64_HWBP_REG(KIND, 13),					\
      AARCH64_HWBP_REG(KIND, 14),					\
      AARCH64_HWBP_REG(KIND, 15),					\
									\
      /* The DBGBVR+DBGBCR pair only takes 12 bytes.  There are 4 bytes	\
	 of padding at the end of each pair.  The item formatter in	\
	 readelf can skip those, but the missing 4 bytes at the end of	\
	 the whole block cause it to assume the whole item bunch	\
	 repeats, so it loops around to read more.  Insert an explicit	\
	 (but invisible) padding word.  */				\
      {									\
	.name = "", .type = ELF_T_WORD, .format = 'h',			\
	.offset = 260, .group = "register"				\
      }									\
    }

AARCH64_BP_WP_GROUP ("B", aarch64_hw_bp_items);
AARCH64_BP_WP_GROUP ("W", aarch64_hw_wp_items);

#undef AARCH64_BP_WP_GROUP
#undef AARCH64_HWBP_REG

#define EXTRA_NOTES							\
  EXTRA_REGSET_ITEMS (NT_FPREGSET, 528,					\
		      aarch64_fpregset_regs, aarch64_fpregset_items)	\
  EXTRA_ITEMS (NT_ARM_TLS, 8, aarch64_tls_items)			\
  EXTRA_ITEMS (NT_ARM_HW_BREAK, 264, aarch64_hw_bp_items)		\
  EXTRA_ITEMS (NT_ARM_HW_WATCH, 264, aarch64_hw_wp_items)		\
  EXTRA_ITEMS (NT_ARM_SYSTEM_CALL, 4, aarch64_syscall_items)

#include "linux-core-note.c"
