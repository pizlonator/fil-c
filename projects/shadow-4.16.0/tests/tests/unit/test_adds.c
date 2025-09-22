// SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <limits.h>

#include <stdarg.h>  // Required by <cmocka.h>
#include <stddef.h>  // Required by <cmocka.h>
#include <setjmp.h>  // Required by <cmocka.h>
#include <stdint.h>  // Required by <cmocka.h>
#include <cmocka.h>

#include "adds.h"


static void test_addsl_2_ok(void **state);
static void test_addsl_2_underflow(void **state);
static void test_addsl_2_overflow(void **state);
static void test_addsl_3_ok(void **state);
static void test_addsl_3_underflow(void **state);
static void test_addsl_3_overflow(void **state);
static void test_addsl_5_ok(void **state);


int
main(void)
{
    const struct CMUnitTest  tests[] = {
        cmocka_unit_test(test_addsl_2_ok),
        cmocka_unit_test(test_addsl_2_underflow),
        cmocka_unit_test(test_addsl_2_overflow),
        cmocka_unit_test(test_addsl_3_ok),
        cmocka_unit_test(test_addsl_3_underflow),
        cmocka_unit_test(test_addsl_3_overflow),
        cmocka_unit_test(test_addsl_5_ok),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}


static void
test_addsl_2_ok(void **state)
{
	assert_true(addsl(1, 3)			== 1 + 3);
	assert_true(addsl(-4321, 7)		== -4321 + 7);
	assert_true(addsl(1, 1)			== 1 + 1);
	assert_true(addsl(-1, -2)		== -1 - 2);
	assert_true(addsl(LONG_MAX, -1)		== LONG_MAX - 1);
	assert_true(addsl(LONG_MIN, 1)		== LONG_MIN + 1);
	assert_true(addsl(LONG_MIN, LONG_MAX)	== LONG_MIN + LONG_MAX);
	assert_true(addsl(0, 0)			== 0);
}


static void
test_addsl_2_underflow(void **state)
{
	assert_true(addsl(LONG_MIN, -1)		== LONG_MIN);
	assert_true(addsl(LONG_MIN + 3, -7)	== LONG_MIN);
	assert_true(addsl(LONG_MIN, LONG_MIN)	== LONG_MIN);
}


static void
test_addsl_2_overflow(void **state)
{
	assert_true(addsl(LONG_MAX, 1)		== LONG_MAX);
	assert_true(addsl(LONG_MAX - 3, 7)	== LONG_MAX);
	assert_true(addsl(LONG_MAX, LONG_MAX)	== LONG_MAX);
}


static void
test_addsl_3_ok(void **state)
{
	assert_true(addsl(1, 2, 3)		== 1 + 2 + 3);
	assert_true(addsl(LONG_MIN, -3, 4)	== LONG_MIN + 4 - 3);
	assert_true(addsl(LONG_MAX, LONG_MAX, LONG_MIN)
						== LONG_MAX + LONG_MIN + LONG_MAX);
}


static void
test_addsl_3_underflow(void **state)
{
	assert_true(addsl(LONG_MIN, 2, -3)	== LONG_MIN);
	assert_true(addsl(LONG_MIN, -1, 0)	== LONG_MIN);
}


static void
test_addsl_3_overflow(void **state)
{
	assert_true(addsl(LONG_MAX, -1, 2)	== LONG_MAX);
	assert_true(addsl(LONG_MAX, +1, 0)	== LONG_MAX);
	assert_true(addsl(LONG_MAX, LONG_MAX, 0)== LONG_MAX);
}


static void
test_addsl_5_ok(void **state)
{
	assert_true(addsl(LONG_MAX, LONG_MAX, LONG_MIN, LONG_MIN, 44) == 42);
}
