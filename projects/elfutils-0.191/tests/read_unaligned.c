/* Test program for read_[type]_unaligned.
   Copyright (C) 2020, Red Hat Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <assert.h>
#include <endian.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../libdw/libdwP.h"
#include "../libdw/memory-access.h"

union u8
{
  uint8_t v;
  unsigned char c[1];
};

union s8
{
  int8_t v;
  unsigned char c[1];
};

union u16
{
  uint16_t v;
  unsigned char c[2];
};

union s16
{
  int16_t v;
  unsigned char c[2];
};

union u24
{
  uint32_t v:24;
  unsigned char c[3];
} __attribute__((packed));

union u32
{
  uint32_t v;
  unsigned char c[4];
};

union s32
{
  int32_t v;
  unsigned char c[4];
};

union u64
{
  uint64_t v;
  unsigned char c[8];
};

union s64
{
  uint64_t v;
  unsigned char c[8];
};

uint8_t u8_nums[] =
  {
   0,
   1,
   UINT8_MAX / 2 - 1,
   UINT8_MAX / 2,
   UINT8_MAX / 2 + 1,
   UINT8_MAX,
   UINT8_MAX -1
  };

int8_t s8_nums[] =
  {
   INT8_MIN,
   INT8_MIN + 1,
   -1,
   0,
   1,
   INT8_MAX,
   INT8_MAX - 1
  };

uint16_t u16_nums[] =
  {
   0,
   1,
   UINT16_MAX / 2 - 1,
   UINT16_MAX / 2,
   UINT16_MAX / 2 + 1,
   UINT16_MAX,
   UINT16_MAX -1
  };

int16_t s16_nums[] =
  {
   INT16_MIN,
   INT16_MIN + 1,
   -1,
   0,
   1,
   INT16_MAX,
   INT16_MAX - 1
  };

#define UINT24_MAX 0xffffff

uint32_t u24_nums[] =
  {
   0,
   1,
   UINT24_MAX / 2 - 1,
   UINT24_MAX / 2,
   UINT24_MAX / 2 + 1,
   UINT24_MAX,
   UINT24_MAX -1
  };

uint32_t u32_nums[] =
  {
   0,
   1,
   UINT32_MAX / 2 - 1,
   UINT32_MAX / 2,
   UINT32_MAX / 2 + 1,
   UINT32_MAX,
   UINT32_MAX -1
  };

int32_t s32_nums[] =
  {
   INT32_MIN,
   INT32_MIN + 1,
   -1,
   0,
   1,
   INT32_MAX,
   INT32_MAX - 1
  };

uint64_t u64_nums[] =
  {
   0,
   1,
   UINT64_MAX / 2 - 1,
   UINT64_MAX / 2,
   UINT64_MAX / 2 + 1,
   UINT64_MAX,
   UINT64_MAX -1
  };

int64_t s64_nums[] =
  {
   INT64_MIN,
   INT64_MIN + 1,
   -1,
   0,
   1,
   INT64_MAX,
   INT64_MAX - 1
  };

static unsigned char le_mem[] =
  {
    /* u8 */
    0x00,
    0x01,
    0x7e,
    0x7f,
    0x80,
    0xff,
    0xfe,
    /* s8 */
    0x80,
    0x81,
    0xff,
    0x00,
    0x01,
    0x7f,
    0x7e,
    /* u16 */
    0x00, 0x00,
    0x01, 0x00,
    0xfe, 0x7f,
    0xff, 0x7f,
    0x00, 0x80,
    0xff, 0xff,
    0xfe, 0xff,
    /* s16 */
    0x00, 0x80,
    0x01, 0x80,
    0xff, 0xff,
    0x00, 0x00,
    0x01, 0x00,
    0xff, 0x7f,
    0xfe, 0x7f,
    /* u24 */
    0x00, 0x00, 0x00,
    0x01, 0x00, 0x00,
    0xfe, 0xff, 0x7f,
    0xff, 0xff, 0x7f,
    0x00, 0x00, 0x80,
    0xff, 0xff, 0xff,
    0xfe, 0xff, 0xff,
    /* u32 */
    0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0xfe, 0xff, 0xff, 0x7f,
    0xff, 0xff, 0xff, 0x7f,
    0x00, 0x00, 0x00, 0x80,
    0xff, 0xff, 0xff, 0xff,
    0xfe, 0xff, 0xff, 0xff,
    /* s32 */
    0x00, 0x00, 0x00, 0x80,
    0x01, 0x00, 0x00, 0x80,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0x7f,
    0xfe, 0xff, 0xff, 0x7f,
    /* u64 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /* s64 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
  };

static unsigned char be_mem[] =
  {
    /* u8 */
    0x00,
    0x01,
    0x7e,
    0x7f,
    0x80,
    0xff,
    0xfe,
    /* s8 */
    0x80,
    0x81,
    0xff,
    0x00,
    0x01,
    0x7f,
    0x7e,
    /* u16 */
    0x00, 0x00,
    0x00, 0x01,
    0x7f, 0xfe,
    0x7f, 0xff,
    0x80, 0x00,
    0xff, 0xff,
    0xff, 0xfe,
    /* s16 */
    0x80, 0x00,
    0x80, 0x01,
    0xff, 0xff,
    0x00, 0x00,
    0x00, 0x01,
    0x7f, 0xff,
    0x7f, 0xfe,
    /* u24 */
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x01,
    0x7f, 0xff, 0xfe,
    0x7f, 0xff, 0xff,
    0x80, 0x00, 0x00,
    0xff, 0xff, 0xff,
    0xff, 0xff, 0xfe,
    /* u32 */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x7f, 0xff, 0xff, 0xfe,
    0x7f, 0xff, 0xff, 0xff,
    0x80, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xfe,
    /* s32 */
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x01,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x7f, 0xff, 0xff, 0xff,
    0x7f, 0xff, 0xff, 0xfe,
    /* u64 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
    /* s64 */
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
  };

int
main (int argc, char **argv __attribute__((unused)))
{
  /* No arguments means check, otherwise Write out the memory array.  */
  bool write = false;
  if (argc > 1)
    write = true;

  bool is_le = (BYTE_ORDER == LITTLE_ENDIAN);

  if (write)
    {
      if (is_le)
	printf ("static unsigned char le_mem[] =\n");
      else
	printf ("static unsigned char be_mem[] =\n");
      printf ("  {\n");
    }

  Dwarf dbg_le = { .other_byte_order = !is_le };
  Dwarf dbg_be = { .other_byte_order = is_le };

  unsigned char *p_le = le_mem;
  unsigned char *p_be = be_mem;

  union u8 u8;
  if (write)
    printf ("    /* u8 */\n");
  for (size_t i = 0; i < sizeof (u8_nums) / sizeof (u8); i++)
    {
      if (write)
	{
	  u8.v = u8_nums[i];
	  printf ("    0x%02" PRIx8 ",\n", u8.c[0]);
	}
      else
	{
	  uint8_t v = *p_le++;
	  assert (v == u8_nums[i]);
	  v = *p_be++;
	  assert (v == u8_nums[i]);
	}
    }

  union s8 s8;
  if (write)
    printf ("    /* s8 */\n");
  for (size_t i = 0; i < sizeof (s8_nums) / sizeof (s8); i++)
    {
      if (write)
	{
	  s8.v = s8_nums[i];
	  printf ("    0x%02" PRIx8 ",\n", s8.c[0]);
	}
      else
	{
	  int8_t v = *p_le++;
	  assert (v == s8_nums[i]);
	  v = *p_be++;
	  assert (v == s8_nums[i]);
	}
    }

  union u16 u16;
  if (write)
    printf ("    /* u16 */\n");
  for (size_t i = 0; i < sizeof (u16_nums) / sizeof (u16); i++)
    {
      if (write)
	{
	  u16.v = u16_nums[i];
	  printf ("    0x%02" PRIx8 ", ", u16.c[0]);
	  printf ("0x%02" PRIx8 ",\n", u16.c[1]);
	}
      else
	{
	  uint16_t v = read_2ubyte_unaligned_inc (&dbg_le, p_le);
	  assert (v == u16_nums[i]);
	  v = read_2ubyte_unaligned_inc (&dbg_be, p_be);
	  assert (v == u16_nums[i]);
	}
    }

  union s16 s16;
  if (write)
    printf ("    /* s16 */\n");
  for (size_t i = 0; i < sizeof (s16_nums) / sizeof (s16); i++)
    {
      if (write)
	{
	  s16.v = s16_nums[i];
	  printf ("    0x%02" PRIx8 ", ", s16.c[0]);
	  printf ("0x%02" PRIx8 ",\n", s16.c[1]);
	}
      else
	{
	  int16_t v = read_2sbyte_unaligned_inc (&dbg_le, p_le);
	  assert (v == s16_nums[i]);
	  v = read_2sbyte_unaligned_inc (&dbg_be, p_be);
	  assert (v == s16_nums[i]);
	}
    }

  union u24 u24;
  if (write)
    printf ("    /* u24 */\n");
  for (size_t i = 0; i < sizeof (u24_nums) / sizeof (uint32_t); i++)
    {
      if (write)
	{
	  u24.v = u24_nums[i];
	  printf ("    0x%02" PRIx8 ", ", u24.c[0]);
	  printf ("0x%02" PRIx8 ", ", u24.c[1]);
	  printf ("0x%02" PRIx8 ",\n", u24.c[2]);
	}
      else
	{
	  uint32_t v = read_3ubyte_unaligned_inc (&dbg_le, p_le);
	  assert (v == u24_nums[i]);
	  v = read_3ubyte_unaligned_inc (&dbg_be, p_be);
	  assert (v == u24_nums[i]);
	}
    }

  union u32 u32;
  if (write)
    printf ("    /* u32 */\n");
  for (size_t i = 0; i < sizeof (u32_nums) / sizeof (u32); i++)
    {
      if (write)
	{
	  u32.v = u32_nums[i];
	  printf ("    0x%02" PRIx8 ", ", u32.c[0]);
	  printf ("0x%02" PRIx8 ", ", u32.c[1]);
	  printf ("0x%02" PRIx8 ", ", u32.c[2]);
	  printf ("0x%02" PRIx8 ",\n", u32.c[3]);
	}
      else
	{
	  uint32_t v = read_4ubyte_unaligned_inc (&dbg_le, p_le);
	  assert (v == u32_nums[i]);
	  v = read_4ubyte_unaligned_inc (&dbg_be, p_be);
	  assert (v == u32_nums[i]);
	}
    }

  union s32 s32;
  if (write)
    printf ("    /* s32 */\n");
  for (size_t i = 0; i < sizeof (s32_nums) / sizeof (s32); i++)
    {
      if (write)
	{
	  s32.v = s32_nums[i];
	  printf ("    0x%02" PRIx8 ", ", s32.c[0]);
	  printf ("0x%02" PRIx8 ", ", s32.c[1]);
	  printf ("0x%02" PRIx8 ", ", s32.c[2]);
	  printf ("0x%02" PRIx8 ",\n", s32.c[3]);
	}
      else
	{
	  int32_t v = read_4sbyte_unaligned_inc (&dbg_le, p_le);
	  assert (v == s32_nums[i]);
	  v = read_4sbyte_unaligned_inc (&dbg_be, p_be);
	  assert (v == s32_nums[i]);
	}
    }

  union u64 u64;
  if (write)
    printf ("    /* u64 */\n");
  for (size_t i = 0; i < sizeof (u64_nums) / sizeof (u64); i++)
    {
      if (write)
	{
	  u64.v = u64_nums[i];
	  printf ("    0x%02" PRIx8 ", ", u64.c[0]);
	  printf ("0x%02" PRIx8 ", ", u64.c[1]);
	  printf ("0x%02" PRIx8 ", ", u64.c[2]);
	  printf ("0x%02" PRIx8 ", ", u64.c[3]);
	  printf ("0x%02" PRIx8 ", ", u64.c[4]);
	  printf ("0x%02" PRIx8 ", ", u64.c[5]);
	  printf ("0x%02" PRIx8 ", ", u64.c[6]);
	  printf ("0x%02" PRIx8 ",\n", u64.c[7]);
	}
      else
	{
	  uint64_t v = read_8ubyte_unaligned_inc (&dbg_le, p_le);
	  assert (v == u64_nums[i]);
	  v = read_8ubyte_unaligned_inc (&dbg_be, p_be);
	  assert (v == u64_nums[i]);
	}
    }

  union s64 s64;
  if (write)
    printf ("    /* s64 */\n");
  for (size_t i = 0; i < sizeof (s64_nums) / sizeof (s64); i++)
    {
      if (write)
	{
	  s64.v = s64_nums[i];
	  printf ("    0x%02" PRIx8 ", ", s64.c[0]);
	  printf ("0x%02" PRIx8 ", ", s64.c[1]);
	  printf ("0x%02" PRIx8 ", ", s64.c[2]);
	  printf ("0x%02" PRIx8 ", ", s64.c[3]);
	  printf ("0x%02" PRIx8 ", ", s64.c[4]);
	  printf ("0x%02" PRIx8 ", ", s64.c[5]);
	  printf ("0x%02" PRIx8 ", ", s64.c[6]);
	  printf ("0x%02" PRIx8 ",\n", s64.c[7]);
	}
      else
	{
	  int64_t v = read_8sbyte_unaligned_inc (&dbg_le, p_le);
	  assert (v == s64_nums[i]);
	  v = read_8sbyte_unaligned_inc (&dbg_be, p_be);
	  assert (v == s64_nums[i]);
	}
    }

  if (write)
    printf ("  };\n");
  else
    {
      assert (p_le == le_mem + sizeof (le_mem));
      assert (p_be == be_mem + sizeof (be_mem));
    }

  return 0;
}
