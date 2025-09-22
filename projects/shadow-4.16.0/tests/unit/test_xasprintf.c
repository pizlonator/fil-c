/*
 * SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>  // Required by <cmocka.h>
#include <stddef.h>  // Required by <cmocka.h>
#include <setjmp.h>  // Required by <cmocka.h>
#include <stdint.h>  // Required by <cmocka.h>
#include <cmocka.h>

#include "string/sprintf.h"


#define assert_unreachable()  assert_true(0)

#define XASPRINTF_CALLED  (-36)
#define EXIT_CALLED       (42)
#define TEST_OK           (-6)


static jmp_buf  jmpb;


/**********************
 * WRAPPERS
 **********************/
int __real_vasprintf(char **restrict p, const char *restrict fmt, va_list ap);
int __wrap_vasprintf(char **restrict p, const char *restrict fmt, va_list ap);
void __wrap_exit(int status);


int
__wrap_vasprintf(char **restrict p, const char *restrict fmt, va_list ap)
{
	return mock() == -1 ? -1 : __real_vasprintf(p, fmt, ap);
}


void
__wrap_exit(int status)
{
	longjmp(jmpb, EXIT_CALLED);
}


/**********************
 * TEST
 **********************/
static void test_xasprintf_exit(void **state);
static void test_xasprintf_ok(void **state);


static void
test_xasprintf_exit(void **state)
{
	volatile int    len;
	char *volatile  p;

	will_return(__wrap_vasprintf, -1);

	len = 0;

	switch (setjmp(jmpb)) {
	case 0:
		len = XASPRINTF_CALLED;
		len = xasprintf(&p, "foo%s", "bar");
		assert_unreachable();
		break;
	case EXIT_CALLED:
		assert_int_equal(len, XASPRINTF_CALLED);
		len = TEST_OK;
		break;
	default:
		assert_unreachable();
		break;
	}

	assert_int_equal(len, TEST_OK);
}


static void
test_xasprintf_ok(void **state)
{
	int   len;
	char  *p;

	// Trick: it will actually return the length, not 0.
	will_return(__wrap_vasprintf, 0);

	len = xasprintf(&p, "foo%d%s", 1, "bar");
	assert_int_equal(len, strlen("foo1bar"));
	assert_string_equal(p, "foo1bar");
	free(p);
}


int
main(void)
{
    const struct CMUnitTest  tests[] = {
        cmocka_unit_test(test_xasprintf_exit),
        cmocka_unit_test(test_xasprintf_ok),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
