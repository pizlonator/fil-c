// SPDX-FileCopyrightText: 2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <config.h>

#include <stddef.h>
#include <string.h>

#include <stdarg.h>  // Required by <cmocka.h>
#include <stddef.h>  // Required by <cmocka.h>
#include <setjmp.h>  // Required by <cmocka.h>
#include <stdint.h>  // Required by <cmocka.h>
#include <cmocka.h>

#include "string/zustr2stp.h"


static void test_ZUSTR2STP(void **state);


int
main(void)
{
    const struct CMUnitTest  tests[] = {
        cmocka_unit_test(test_ZUSTR2STP),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}


static void
test_ZUSTR2STP(void **state)
{
	char  src[3] = {'1', '2', '3'};
	char  dst[4];

	assert_true(ZUSTR2STP(&dst, src) == dst + strlen("123"));
	assert_true(strcmp("123", dst) == 0);

	src[2] = '\0';
	assert_true(ZUSTR2STP(&dst, src) == dst + strlen("12"));
	assert_true(strcmp("12", dst) == 0);

	src[1] = '\0';
	assert_true(ZUSTR2STP(&dst, src) == dst + strlen("1"));
	assert_true(strcmp("1", dst) == 0);

	src[0] = '\0';
	assert_true(ZUSTR2STP(&dst, src) == dst + strlen(""));
	assert_true(strcmp("", dst) == 0);
}
