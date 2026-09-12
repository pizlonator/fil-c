/* Tests for __ifunc_hwcap helper function.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
   <https://www.gnu.org/licenses/>.  */

#include <stdint.h>
#include <sys/ifunc.h>
#include <support/check.h>

#define CHECK_VALUES_WITH_ARG(p1, p2, p3, p4) \
  ({ \
    TEST_COMPARE (__ifunc_hwcap (0, _IFUNC_ARG_HWCAP, arg), 0); \
    TEST_COMPARE (__ifunc_hwcap (_IFUNC_ARG_AT_HWCAP, _IFUNC_ARG_HWCAP, arg), p1); \
    TEST_COMPARE (__ifunc_hwcap (_IFUNC_ARG_AT_HWCAP2, _IFUNC_ARG_HWCAP, arg), p2); \
    TEST_COMPARE (__ifunc_hwcap (_IFUNC_ARG_AT_HWCAP3, _IFUNC_ARG_HWCAP, arg), p3); \
    TEST_COMPARE (__ifunc_hwcap (_IFUNC_ARG_AT_HWCAP4, _IFUNC_ARG_HWCAP, arg), p4); \
    TEST_COMPARE (__ifunc_hwcap (5, _IFUNC_ARG_HWCAP, arg), 0); \
  })

#define CHECK_VALUES_WITHOUT_ARG(p1) \
  ({ \
    TEST_COMPARE (__ifunc_hwcap (0, p1, arg), 0); \
    TEST_COMPARE (__ifunc_hwcap (_IFUNC_ARG_AT_HWCAP, p1, arg), p1); \
    TEST_COMPARE (__ifunc_hwcap (_IFUNC_ARG_AT_HWCAP2, p1, arg), 0); \
    TEST_COMPARE (__ifunc_hwcap (_IFUNC_ARG_AT_HWCAP3, p1, arg), 0); \
    TEST_COMPARE (__ifunc_hwcap (_IFUNC_ARG_AT_HWCAP4, p1, arg), 0); \
    TEST_COMPARE (__ifunc_hwcap (5, p1, arg), 0); \
  })

static void
test_one (const unsigned long *arg)
{
  uint64_t size = arg[0] / sizeof (uint64_t);

  switch (size)
    {
      case 1:
	CHECK_VALUES_WITH_ARG (0, 0, 0, 0);
	CHECK_VALUES_WITHOUT_ARG (0);
	break;
      case 2:
	CHECK_VALUES_WITH_ARG (1, 0, 0, 0);
	CHECK_VALUES_WITHOUT_ARG (1);
	break;
      case 3:
	CHECK_VALUES_WITH_ARG (1, 2, 0, 0);
	CHECK_VALUES_WITHOUT_ARG (1);
	break;
      case 4:
	CHECK_VALUES_WITH_ARG (1, 2, 3, 0);
	CHECK_VALUES_WITHOUT_ARG (1);
	break;
      case 5:
	CHECK_VALUES_WITH_ARG (1, 2, 3, 4);
	CHECK_VALUES_WITHOUT_ARG (1);
	break;
      default:
	TEST_VERIFY (0); // unexpected size
	break;
    }
}

static int
do_test (void)
{
  uint64_t arg[_IFUNC_HWCAP_MAX + 1] = {
    0, /* Placeholder for size */
    _IFUNC_ARG_AT_HWCAP, /* AT_HWCAP */
    _IFUNC_ARG_AT_HWCAP2, /* AT_HWCAP2 */
    _IFUNC_ARG_AT_HWCAP3, /* AT_HWCAP3 */
    _IFUNC_ARG_AT_HWCAP4, /* AT_HWCAP4 */
  };

  for (int k = 0; k <= _IFUNC_HWCAP_MAX; k++)
    {
      /* Update size */
      arg[0] = (k + 1) * sizeof (uint64_t);
      test_one (arg);
    }

  return 0;
}

#include <support/test-driver.c>
