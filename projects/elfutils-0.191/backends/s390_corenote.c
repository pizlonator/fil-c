/* S390-specific core note handling.
   Copyright (C) 2012 Red Hat, Inc.
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
# define BACKEND	s390_
#else
# define BITS 		64
# define BACKEND	s390x_
#endif
#include "libebl_CPU.h"

static const Ebl_Register_Location prstatus_regs[] =
  {
#define GR(at, n, dwreg, b...)						\
    { .offset = at * BITS/8, .regno = dwreg, .count = n, .bits = b }

    GR ( 0,  1, 64, BITS),				/* pswm */
    GR ( 1,  1, 65, BITS, .pc_register = true ),	/* pswa */
    GR ( 2, 16,  0, BITS),				/* r0-r15 */
    GR (18, 16, 48,   32),				/* ar0-ar15 */

#undef	GR
  };

  /* orig_r2 is at offset (BITS == 32 ? 34 * 4 : 26 * 8).  */
#define PRSTATUS_REGS_SIZE	(BITS / 8 * (BITS == 32 ? 35 : 27))

static const Ebl_Register_Location fpregset_regs[] =
  {
#define FPR(at, n, dwreg)						\
    { .offset = at * 64/8, .regno = dwreg, .count = n, .bits = 64 }

    /* fpc is at offset 0, see fpregset_items, it has no assigned DWARF regno.
       Bytes at offsets 4 to 7 are unused.  */
    FPR (1 +  0, 1, 16),	/* f0 */
    FPR (1 +  1, 1, 20),	/* f1 */
    FPR (1 +  2, 1, 17),	/* f2 */
    FPR (1 +  3, 1, 21),	/* f3 */
    FPR (1 +  4, 1, 18),	/* f4 */
    FPR (1 +  5, 1, 22),	/* f5 */
    FPR (1 +  6, 1, 19),	/* f6 */
    FPR (1 +  7, 1, 23),	/* f7 */
    FPR (1 +  8, 1, 24),	/* f8 */
    FPR (1 +  9, 1, 28),	/* f9 */
    FPR (1 + 10, 1, 25),	/* f10 */
    FPR (1 + 11, 1, 29),	/* f11 */
    FPR (1 + 12, 1, 26),	/* f12 */
    FPR (1 + 13, 1, 30),	/* f13 */
    FPR (1 + 14, 1, 27),	/* f14 */
    FPR (1 + 15, 1, 31),	/* f15 */

#undef	FPR
  };

static const Ebl_Core_Item fpregset_items[] =
  {
    {
      .name = "fpc", .group = "register", .offset = 0, .type = ELF_T_WORD,
      .format = 'x',
    },
  };

/* Do not set FPREGSET_SIZE so that we can supply fpregset_items.  */
#define EXTRA_NOTES_FPREGSET \
    EXTRA_REGSET_ITEMS (NT_FPREGSET, 17 * 8, fpregset_regs, fpregset_items)

#if BITS == 32
# define ULONG			uint32_t
# define ALIGN_ULONG		4
# define TYPE_ULONG		ELF_T_WORD
# define TYPE_LONG		ELF_T_SWORD
# define UID_T			uint16_t
# define GID_T			uint16_t
# define ALIGN_UID_T		2
# define ALIGN_GID_T		2
# define TYPE_UID_T		ELF_T_HALF
# define TYPE_GID_T		ELF_T_HALF
#else
# define ULONG			uint64_t
# define ALIGN_ULONG		8
# define TYPE_ULONG		ELF_T_XWORD
# define TYPE_LONG		ELF_T_SXWORD
# define UID_T			uint32_t
# define GID_T			uint32_t
# define ALIGN_UID_T		4
# define ALIGN_GID_T		4
# define TYPE_UID_T		ELF_T_WORD
# define TYPE_GID_T		ELF_T_WORD
#endif
#define PID_T			int32_t
#define ALIGN_PID_T		4
#define TYPE_PID_T		ELF_T_SWORD
/* s390 psw_compat_t has alignment 8 bytes where it is inherited from.  */
#define ALIGN_PR_REG		8

#define PRSTATUS_REGSET_ITEMS					\
  {								\
    .name = "orig_r2", .type = TYPE_LONG, .format = 'd',	\
    .offset = offsetof (struct EBLHOOK(prstatus),		\
			pr_reg[BITS == 32 ? 34 : 26]),		\
    .group = "register"						\
  }

#if BITS == 32

static const Ebl_Core_Item high_regs_items[] =
  {
#define HR(n)								\
    {									\
      .name = "high_r" #n , .group = "register", .offset = (n) * 4,	\
      .type = ELF_T_WORD, .format = 'x',				\
    }

    /* Upper halves of r0-r15 are stored here.
       FIXME: They are currently not combined with the r0-r15 lower halves.  */
    HR (0), HR (1), HR (2), HR (3), HR (4), HR (5), HR (6), HR (7),
    HR (8), HR (9), HR (10), HR (11), HR (12), HR (13), HR (14), HR (15)

#undef HR
  };

#define EXTRA_NOTES_HIGH_GPRS \
  EXTRA_ITEMS (NT_S390_HIGH_GPRS, 16 * 4, high_regs_items)

#else /* BITS == 64 */

#define EXTRA_NOTES_HIGH_GPRS

#endif /* BITS == 64 */

static const Ebl_Core_Item last_break_items[] =
  {
    {
      .name = "last_break", .group = "system", .offset = BITS == 32 ? 4 : 0,
      .type = BITS == 32 ? ELF_T_WORD : ELF_T_XWORD, .format = 'x',
    },
  };

static const Ebl_Core_Item system_call_items[] =
  {
    {
      .name = "system_call", .group = "system", .offset = 0, .type = ELF_T_WORD,
      .format = 'd',
    },
  };

#define	EXTRA_NOTES							  \
  EXTRA_NOTES_FPREGSET							  \
  EXTRA_NOTES_HIGH_GPRS							  \
  EXTRA_ITEMS (NT_S390_LAST_BREAK, 8, last_break_items)	  \
  EXTRA_ITEMS (NT_S390_SYSTEM_CALL, 4, system_call_items)

#include "linux-core-note.c"
