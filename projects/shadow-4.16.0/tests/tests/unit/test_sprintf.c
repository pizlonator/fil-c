// SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <config.h>

#include <stdio.h>
#include <string.h>

#include <stdarg.h>  // Required by <cmocka.h>
#include <stddef.h>  // Required by <cmocka.h>
#include <setjmp.h>  // Required by <cmocka.h>
#include <stdint.h>  // Required by <cmocka.h>
#include <cmocka.h>

#include "sizeof.h"
#include "string/sprintf.h"


static void test_SNPRINTF_trunc(void **state);
static void test_SNPRINTF_ok(void **state);


int
main(void)
{
    const struct CMUnitTest  tests[] = {
        cmocka_unit_test(test_SNPRINTF_trunc),
        cmocka_unit_test(test_SNPRINTF_ok),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}


static void
test_SNPRINTF_trunc(void **state)
{
	char  buf[NITEMS("foo")];

	// Test that we're not returning SIZE_MAX
	assert_true(SNPRINTF(buf, "f%su", "oo") < 0);
	assert_true(strcmp(buf, "foo") == 0);

	assert_true(SNPRINTF(buf, "barbaz") == -1);
	assert_true(strcmp(buf, "bar") == 0);
}


static void
test_SNPRINTF_ok(void **state)
{
	char  buf[NITEMS("foo")];

	assert_true(SNPRINTF(buf, "%s", "foo") == strlen("foo"));
	assert_true(strcmp(buf, "foo") == 0);

	assert_true(SNPRINTF(buf, "%do", 1) == strlen("1o"));
	assert_true(strcmp(buf, "1o") == 0);

	assert_true(SNPRINTF(buf, "f") == strlen("f"));
	assert_true(strcmp(buf, "f") == 0);

	assert_true(SNPRINTF(buf, "") == strlen(""));
	assert_true(strcmp(buf, "") == 0);
}
