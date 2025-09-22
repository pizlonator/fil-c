/*
 * SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <config.h>

#include <string.h>

#include <stdarg.h>  // Required by <cmocka.h>
#include <stddef.h>  // Required by <cmocka.h>
#include <setjmp.h>  // Required by <cmocka.h>
#include <stdint.h>  // Required by <cmocka.h>
#include <cmocka.h>

#include "sizeof.h"
#include "string/strncpy.h"


static void test_STRNCPY_trunc(void **state);
static void test_STRNCPY_fit(void **state);
static void test_STRNCPY_pad(void **state);


int
main(void)
{
    const struct CMUnitTest  tests[] = {
        cmocka_unit_test(test_STRNCPY_trunc),
        cmocka_unit_test(test_STRNCPY_fit),
        cmocka_unit_test(test_STRNCPY_pad),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}


static void
test_STRNCPY_trunc(void **state)
{
	char  buf[3];

	char  src1[4] = {'f', 'o', 'o', 'o'};
	char  res1[3] = {'f', 'o', 'o'};
	assert_true(memcmp(res1, STRNCPY(buf, src1), sizeof(buf)) == 0);

	char  src2[5] = "barb";
	char  res2[3] = {'b', 'a', 'r'};
	assert_true(memcmp(res2, STRNCPY(buf, src2), sizeof(buf)) == 0);
}


static void
test_STRNCPY_fit(void **state)
{
	char  buf[3];

	char  src1[3] = {'b', 'a', 'z'};
	char  res1[3] = {'b', 'a', 'z'};
	assert_true(memcmp(res1, STRNCPY(buf, src1), sizeof(buf)) == 0);

	char  src2[4] = "qwe";
	char  res2[3] = {'q', 'w', 'e'};
	assert_true(memcmp(res2, STRNCPY(buf, src2), sizeof(buf)) == 0);
}


static void
test_STRNCPY_pad(void **state)
{
	char  buf[3];

	char  src1[3] = "as";
	char  res1[3] = {'a', 's', 0};
	assert_true(memcmp(res1, STRNCPY(buf, src1), sizeof(buf)) == 0);

	char  src2[3] = "";
	char  res2[3] = {0, 0, 0};
	assert_true(memcmp(res2, STRNCPY(buf, src2), sizeof(buf)) == 0);

	char  src3[3] = {'a', 0, 'b'};
	char  res3[3] = {'a', 0, 0};
	assert_true(memcmp(res3, STRNCPY(buf, src3), sizeof(buf)) == 0);
}
