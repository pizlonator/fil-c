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

#include <stdint.h>
#include <string.h>
#include <x86intrin.h>
#include <support/check.h>

#define MAX_ROWS 16
#define MAX_COLS 64
#define MAX 1024
#define STRIDE 64

typedef struct __tile_config
{
  uint8_t palette_id;
  uint8_t start_row;
  uint8_t reserved_0[14];
  uint16_t colsb[16];
  uint8_t rows[16];
} __tilecfg __attribute__ ((aligned (64)));

/* Initialize int8_t buffer */
static inline void
init_buffer (int8_t *buf, int8_t value)
{
  int rows, colsb, i, j;
  rows  = MAX_ROWS;
  colsb = MAX_COLS;

  for (i = 0; i < rows; i++)
    for (j = 0; j < colsb; j++)
      buf[i * colsb + j] = value;
}

#define BEFORE_TLSDESC_CALL()					\
  int8_t src[MAX];						\
  int8_t res[MAX];						\
  /* Initialize src with data  */				\
  init_buffer (src, 2);						\
  /* Load tile rows from memory.  */				\
  _tile_loadd (2, src, STRIDE);

#define AFTER_TLSDESC_CALL()					\
  /* Store the tile data to memory.  */				\
  _tile_stored (2, res, STRIDE);				\
  _tile_release ();						\
  TEST_VERIFY_EXIT (memcmp (src, res, sizeof (res)) == 0);
