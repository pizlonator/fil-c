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
#include "string/strtcpy.h"


static void test_STRTCPY_trunc(void **state);
static void test_STRTCPY_ok(void **state);


int
main(void)
{
    const struct CMUnitTest  tests[] = {
        cmocka_unit_test(test_STRTCPY_trunc),
        cmocka_unit_test(test_STRTCPY_ok),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}


static void
test_STRTCPY_trunc(void **state)
{
	char  buf[NITEMS("foo")];

	// Test that we're not returning SIZE_MAX
	assert_true(STRTCPY(buf, "fooo") < 0);
	assert_string_equal(buf, "foo");

	assert_int_equal(STRTCPY(buf, "barbaz"), -1);
	assert_string_equal(buf, "bar");
}


static void
test_STRTCPY_ok(void **state)
{
	char  buf[NITEMS("foo")];

	assert_int_equal(STRTCPY(buf, "foo"), strlen("foo"));
	assert_string_equal(buf, "foo");

	assert_int_equal(STRTCPY(buf, "fo"), strlen("fo"));
	assert_string_equal(buf, "fo");

	assert_int_equal(STRTCPY(buf, "f"), strlen("f"));
	assert_string_equal(buf, "f");

	assert_int_equal(STRTCPY(buf, ""), strlen(""));
	assert_string_equal(buf, "");
}
