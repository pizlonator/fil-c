/* Test TLSDESC relocation with AMX.
   Copyright (C) 2024 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include <stdbool.h>
#include <asm/prctl.h>
#include <support/check.h>
#include "tst-gnu2-tls2-amx.h"

extern int arch_prctl (int, ...);

#define X86_XSTATE_TILECFG_ID	17
#define X86_XSTATE_TILEDATA_ID	18

/* Initialize tile config.  */
__attribute__ ((noinline, noclone))
static void
init_tile_config (__tilecfg *tileinfo)
{
  int i;
  tileinfo->palette_id = 1;
  tileinfo->start_row = 0;

  tileinfo->colsb[0] = MAX_ROWS;
  tileinfo->rows[0] = MAX_ROWS;

  for (i = 1; i < 4; ++i)
  {
    tileinfo->colsb[i] = MAX_COLS;
    tileinfo->rows[i] = MAX_ROWS;
  }

  _tile_loadconfig (tileinfo);
}

static bool
enable_amx (void)
{
  uint64_t bitmask;
  if (arch_prctl (ARCH_GET_XCOMP_PERM, &bitmask) != 0)
    return false;

  if ((bitmask & (1 << X86_XSTATE_TILECFG_ID)) == 0)
    return false;

  if (arch_prctl (ARCH_REQ_XCOMP_PERM, X86_XSTATE_TILEDATA_ID) != 0)
    return false;

  /* Load tile configuration.  */
  __tilecfg tile_data = { 0 };
  init_tile_config (&tile_data);

  return true;
}

/* An architecture can define it to clobber caller-saved registers in
   malloc below to verify that the implicit TLSDESC call won't change
   caller-saved registers.  */
static void
clear_tile_register (void)
{
  _tile_zero (2);
}

#define MOD(i) "tst-gnu2-tls2-amx-mod" #i ".so"
#define IS_SUPPORTED()	enable_amx ()
#define PREPARE_MALLOC() clear_tile_register ()

#include <elf/tst-gnu2-tls2.c>
