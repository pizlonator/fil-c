/* Test of <byteswap.h> substitute.
   Copyright (C) 2007-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2007.  */

#include <config.h>

#include <byteswap.h>
#include <stdint.h>

#include "macros.h"

/* Test bswap functions with constant values.  */
static void
test_bswap_constant (void)
{
  ASSERT (bswap_16 (UINT16_C (0x1234)) == UINT16_C (0x3412));
  ASSERT (bswap_32 (UINT32_C (0x12345678)) == UINT32_C (0x78563412));
#ifdef UINT_LEAST64_MAX
  ASSERT (bswap_64 (UINT64_C (0x1234567890ABCDEF))
          == UINT64_C (0xEFCDAB9078563412));
#endif
}

/* Test that the bswap functions evaluate their arguments once.  */
static void
test_bswap_eval_once (void)
{
  uint_least16_t value_1 = 0;
  ASSERT (bswap_16 (value_1++) == 0);
  ASSERT (value_1 == 1);

  uint_least32_t value_2 = 0;
  ASSERT (bswap_32 (value_2++) == 0);
  ASSERT (value_2 == 1);

#ifdef UINT_LEAST64_MAX
  uint_least64_t value_3 = 0;
  ASSERT (bswap_64 (value_3++) == 0);
  ASSERT (value_3 == 1);
#endif
}

/* Test that the bswap functions accept floating-point arguments.  */
static void
test_bswap_double (void)
{
  ASSERT (bswap_16 (0.0) == 0);
  ASSERT (bswap_32 (0.0) == 0);
#ifdef UINT_LEAST64_MAX
  ASSERT (bswap_64 (0.0) == 0);
#endif
}

int
main (void)
{
  test_bswap_constant ();
  test_bswap_eval_once ();
  test_bswap_double ();

  return test_exit_status;
}
