/* Test the stdc_rotate_right_* functions and macro.
   Copyright (C) 2026 Free Software Foundation, Inc.

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
   <https://www.gnu.org/licenses/>.  */

#include <stdbit.h>
#include <support/check.h>

#define TEST_CASE(type, function, value, shift, expect)         \
  do                                                            \
    {                                                           \
      type v = value;                                           \
      TEST_COMPARE (function (v, shift), expect);               \
      TEST_COMPARE (stdc_rotate_right (v, shift), expect);      \
    }                                                           \
  while (false)

static void
test_stdc_rotate_right_uc (void)
{
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 0, 0x96);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 1, 0x4b);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 2, 0xa5);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 3, 0xd2);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 4, 0x69);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 5, 0xb4);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 6, 0x5a);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 7, 0x2d);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 8, 0x96);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 9, 0x4b);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 10, 0xa5);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 11, 0xd2);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 12, 0x69);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 13, 0xb4);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 14, 0x5a);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 15, 0x2d);
  TEST_CASE (unsigned char, stdc_rotate_right_uc, 0x96, 16, 0x96);
}

static void
test_stdc_rotate_right_us (void)
{
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 0, 0x5e1aU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 1, 0x2f0dU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 2, 0x9786U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 3, 0x4bc3U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 4, 0xa5e1U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 5, 0xd2f0U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 6, 0x6978U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 7, 0x34bcU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 8, 0x1a5eU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 9, 0x0d2fU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 10, 0x8697U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 11, 0xc34bU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 12, 0xe1a5U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 13, 0xf0d2U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 14, 0x7869U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 15, 0xbc34U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 16, 0x5e1aU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 17, 0x2f0dU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 18, 0x9786U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 19, 0x4bc3U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 20, 0xa5e1U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 21, 0xd2f0U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 22, 0x6978U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 23, 0x34bcU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 24, 0x1a5eU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 25, 0x0d2fU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 26, 0x8697U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 27, 0xc34bU);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 28, 0xe1a5U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 29, 0xf0d2U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 30, 0x7869U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 31, 0xbc34U);
  TEST_CASE (unsigned short int, stdc_rotate_right_us, 0x5e1aU, 32, 0x5e1aU);
}

#define TEST_CASES_32(type, function)                                   \
  do                                                                    \
    {                                                                   \
      TEST_CASE (type, function, 0x51af3678UL, 0, 0x51af3678UL);        \
      TEST_CASE (type, function, 0x51af3678UL, 1, 0x28d79b3cUL);        \
      TEST_CASE (type, function, 0x51af3678UL, 2, 0x146bcd9eUL);        \
      TEST_CASE (type, function, 0x51af3678UL, 3, 0x0a35e6cfUL);        \
      TEST_CASE (type, function, 0x51af3678UL, 4, 0x851af367UL);        \
      TEST_CASE (type, function, 0x51af3678UL, 5, 0xc28d79b3UL);        \
      TEST_CASE (type, function, 0x51af3678UL, 6, 0xe146bcd9UL);        \
      TEST_CASE (type, function, 0x51af3678UL, 7, 0xf0a35e6cUL);        \
      TEST_CASE (type, function, 0x51af3678UL, 8, 0x7851af36UL);        \
      TEST_CASE (type, function, 0x51af3678UL, 9, 0x3c28d79bUL);        \
      TEST_CASE (type, function, 0x51af3678UL, 10, 0x9e146bcdUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 11, 0xcf0a35e6UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 12, 0x67851af3UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 13, 0xb3c28d79UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 14, 0xd9e146bcUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 15, 0x6cf0a35eUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 16, 0x367851afUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 17, 0x9b3c28d7UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 18, 0xcd9e146bUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 19, 0xe6cf0a35UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 20, 0xf367851aUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 21, 0x79b3c28dUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 22, 0xbcd9e146UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 23, 0x5e6cf0a3UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 24, 0xaf367851UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 25, 0xd79b3c28UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 26, 0x6bcd9e14UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 27, 0x35e6cf0aUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 28, 0x1af36785UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 29, 0x8d79b3c2UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 30, 0x46bcd9e1UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 31, 0xa35e6cf0UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 32, 0x51af3678UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 33, 0x28d79b3cUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 34, 0x146bcd9eUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 35, 0x0a35e6cfUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 36, 0x851af367UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 37, 0xc28d79b3UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 38, 0xe146bcd9UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 39, 0xf0a35e6cUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 40, 0x7851af36UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 41, 0x3c28d79bUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 42, 0x9e146bcdUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 43, 0xcf0a35e6UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 44, 0x67851af3UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 45, 0xb3c28d79UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 46, 0xd9e146bcUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 47, 0x6cf0a35eUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 48, 0x367851afUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 49, 0x9b3c28d7UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 50, 0xcd9e146bUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 51, 0xe6cf0a35UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 52, 0xf367851aUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 53, 0x79b3c28dUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 54, 0xbcd9e146UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 55, 0x5e6cf0a3UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 56, 0xaf367851UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 57, 0xd79b3c28UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 58, 0x6bcd9e14UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 59, 0x35e6cf0aUL);       \
      TEST_CASE (type, function, 0x51af3678UL, 60, 0x1af36785UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 61, 0x8d79b3c2UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 62, 0x46bcd9e1UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 63, 0xa35e6cf0UL);       \
      TEST_CASE (type, function, 0x51af3678UL, 64, 0x51af3678UL);       \
    }                                                                   \
  while (false)

static void
test_stdc_rotate_right_ui (void)
{
  TEST_CASES_32 (unsigned int, stdc_rotate_right_ui);
}

#define TEST_CASES_64(type, function)                                   \
  do                                                                    \
    {                                                                   \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 0,              \
                 0x59ae28915a84db37ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 1,              \
                 0xacd71448ad426d9bULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 2,              \
                 0xd66b8a2456a136cdULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 3,              \
                 0xeb35c5122b509b66ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 4,              \
                 0x759ae28915a84db3ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 5,              \
                 0xbacd71448ad426d9ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 6,              \
                 0xdd66b8a2456a136cULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 7,              \
                 0x6eb35c5122b509b6ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 8,              \
                 0x3759ae28915a84dbULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 9,              \
                 0x9bacd71448ad426dULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 10,             \
                 0xcdd66b8a2456a136ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 11,             \
                 0x66eb35c5122b509bULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 12,             \
                 0xb3759ae28915a84dULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 13,             \
                 0xd9bacd71448ad426ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 14,             \
                 0x6cdd66b8a2456a13ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 15,             \
                 0xb66eb35c5122b509ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 16,             \
                 0xdb3759ae28915a84ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 17,             \
                 0x6d9bacd71448ad42ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 18,             \
                 0x36cdd66b8a2456a1ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 19,             \
                 0x9b66eb35c5122b50ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 20,             \
                 0x4db3759ae28915a8ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 21,             \
                 0x26d9bacd71448ad4ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 22,             \
                 0x136cdd66b8a2456aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 23,             \
                 0x09b66eb35c5122b5ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 24,             \
                 0x84db3759ae28915aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 25,             \
                 0x426d9bacd71448adULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 26,             \
                 0xa136cdd66b8a2456ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 27,             \
                 0x509b66eb35c5122bULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 28,             \
                 0xa84db3759ae28915ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 29,             \
                 0xd426d9bacd71448aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 30,             \
                 0x6a136cdd66b8a245ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 31,             \
                 0xb509b66eb35c5122ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 32,             \
                 0x5a84db3759ae2891ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 33,             \
                 0xad426d9bacd71448ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 34,             \
                 0x56a136cdd66b8a24ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 35,             \
                 0x2b509b66eb35c512ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 36,             \
                 0x15a84db3759ae289ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 37,             \
                 0x8ad426d9bacd7144ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 38,             \
                 0x456a136cdd66b8a2ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 39,             \
                 0x22b509b66eb35c51ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 40,             \
                 0x915a84db3759ae28ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 41,             \
                 0x48ad426d9bacd714ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 42,             \
                 0x2456a136cdd66b8aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 43,             \
                 0x122b509b66eb35c5ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 44,             \
                 0x8915a84db3759ae2ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 45,             \
                 0x448ad426d9bacd71ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 46,             \
                 0xa2456a136cdd66b8ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 47,             \
                 0x5122b509b66eb35cULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 48,             \
                 0x28915a84db3759aeULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 49,             \
                 0x1448ad426d9bacd7ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 50,             \
                 0x8a2456a136cdd66bULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 51,             \
                 0xc5122b509b66eb35ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 52,             \
                 0xe28915a84db3759aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 53,             \
                 0x71448ad426d9bacdULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 54,             \
                 0xb8a2456a136cdd66ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 55,             \
                 0x5c5122b509b66eb3ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 56,             \
                 0xae28915a84db3759ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 57,             \
                 0xd71448ad426d9bacULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 58,             \
                 0x6b8a2456a136cdd6ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 59,             \
                 0x35c5122b509b66ebULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 60,             \
                 0x9ae28915a84db375ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 61,             \
                 0xcd71448ad426d9baULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 62,             \
                 0x66b8a2456a136cddULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 63,             \
                 0xb35c5122b509b66eULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 64,             \
                 0x59ae28915a84db37ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 65,             \
                 0xacd71448ad426d9bULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 66,             \
                 0xd66b8a2456a136cdULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 67,             \
                 0xeb35c5122b509b66ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 68,             \
                 0x759ae28915a84db3ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 69,             \
                 0xbacd71448ad426d9ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 70,             \
                 0xdd66b8a2456a136cULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 71,             \
                 0x6eb35c5122b509b6ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 72,             \
                 0x3759ae28915a84dbULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 73,             \
                 0x9bacd71448ad426dULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 74,             \
                 0xcdd66b8a2456a136ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 75,             \
                 0x66eb35c5122b509bULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 76,             \
                 0xb3759ae28915a84dULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 77,             \
                 0xd9bacd71448ad426ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 78,             \
                 0x6cdd66b8a2456a13ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 79,             \
                 0xb66eb35c5122b509ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 80,             \
                 0xdb3759ae28915a84ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 81,             \
                 0x6d9bacd71448ad42ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 82,             \
                 0x36cdd66b8a2456a1ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 83,             \
                 0x9b66eb35c5122b50ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 84,             \
                 0x4db3759ae28915a8ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 85,             \
                 0x26d9bacd71448ad4ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 86,             \
                 0x136cdd66b8a2456aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 87,             \
                 0x09b66eb35c5122b5ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 88,             \
                 0x84db3759ae28915aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 89,             \
                 0x426d9bacd71448adULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 90,             \
                 0xa136cdd66b8a2456ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 91,             \
                 0x509b66eb35c5122bULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 92,             \
                 0xa84db3759ae28915ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 93,             \
                 0xd426d9bacd71448aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 94,             \
                 0x6a136cdd66b8a245ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 95,             \
                 0xb509b66eb35c5122ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 96,             \
                 0x5a84db3759ae2891ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 97,             \
                 0xad426d9bacd71448ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 98,             \
                 0x56a136cdd66b8a24ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 99,             \
                 0x2b509b66eb35c512ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 100,            \
                 0x15a84db3759ae289ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 101,            \
                 0x8ad426d9bacd7144ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 102,            \
                 0x456a136cdd66b8a2ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 103,            \
                 0x22b509b66eb35c51ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 104,            \
                 0x915a84db3759ae28ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 105,            \
                 0x48ad426d9bacd714ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 106,            \
                 0x2456a136cdd66b8aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 107,            \
                 0x122b509b66eb35c5ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 108,            \
                 0x8915a84db3759ae2ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 109,            \
                 0x448ad426d9bacd71ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 110,            \
                 0xa2456a136cdd66b8ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 111,            \
                 0x5122b509b66eb35cULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 112,            \
                 0x28915a84db3759aeULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 113,            \
                 0x1448ad426d9bacd7ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 114,            \
                 0x8a2456a136cdd66bULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 115,            \
                 0xc5122b509b66eb35ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 116,            \
                 0xe28915a84db3759aULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 117,            \
                 0x71448ad426d9bacdULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 118,            \
                 0xb8a2456a136cdd66ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 119,            \
                 0x5c5122b509b66eb3ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 120,            \
                 0xae28915a84db3759ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 121,            \
                 0xd71448ad426d9bacULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 122,            \
                 0x6b8a2456a136cdd6ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 123,            \
                 0x35c5122b509b66ebULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 124,            \
                 0x9ae28915a84db375ULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 125,            \
                 0xcd71448ad426d9baULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 126,            \
                 0x66b8a2456a136cddULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 127,            \
                 0xb35c5122b509b66eULL);                                \
      TEST_CASE (type, function, 0x59ae28915a84db37ULL, 128,            \
                 0x59ae28915a84db37ULL);                                \
    }                                                                   \
  while (false)

static void
test_stdc_rotate_right_ul (void)
{
  if (sizeof 0ul < sizeof 0ull)
    TEST_CASES_32 (unsigned long int, stdc_rotate_right_ul);
  else
    TEST_CASES_64 (unsigned long long int, stdc_rotate_right_ul);
}

static void
test_stdc_rotate_right_ull (void)
{
  TEST_CASES_64 (unsigned long long int, stdc_rotate_right_ull);
}

static int
do_test (void)
{
  test_stdc_rotate_right_uc ();
  test_stdc_rotate_right_us ();
  test_stdc_rotate_right_ui ();
  test_stdc_rotate_right_ul ();
  test_stdc_rotate_right_ull ();
  return 0;
}

#include <support/test-driver.c>
