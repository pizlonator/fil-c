// SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <stdarg.h>  // Required by <cmocka.h>
#include <stddef.h>  // Required by <cmocka.h>
#include <setjmp.h>  // Required by <cmocka.h>
#include <stdint.h>  // Required by <cmocka.h>
#include <cmocka.h>

#include "atoi/strtoi.h"
#include "atoi/strtou_noneg.h"


static void test_strtoi(void **state);
static void test_strtou(void **state);
static void test_strtou_noneg(void **state);


int
main(void)
{
    const struct CMUnitTest  tests[] = {
        cmocka_unit_test(test_strtoi),
        cmocka_unit_test(test_strtou),
        cmocka_unit_test(test_strtou_noneg),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}


static void
test_strtoi(void **state)
{
	int   status;
	char  *end;

	errno = 0;
	assert_true(strtoi_("42", NULL, -1, 1, 2, &status) == 1);
	assert_true(status == EINVAL);

	assert_true(strtoi_("40", &end, 5, INTMAX_MIN, INTMAX_MAX, &status) == 20);
	assert_true(status == 0);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtoi_("-40", &end, 0, INTMAX_MIN, INTMAX_MAX, &status) == -40);
	assert_true(status == 0);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtoi_("z", &end, 0, INTMAX_MIN, INTMAX_MAX, &status) == 0);
	assert_true(status == ECANCELED);
	assert_true(strcmp(end, "z") == 0);

	assert_true(strtoi_(" 5", &end, 0, 3, 4, &status) == 4);
	assert_true(status == ERANGE);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtoi_("5z", &end, 0, INTMAX_MIN, INTMAX_MAX, &status) == 5);
	assert_true(status == ENOTSUP);
	assert_true(strcmp(end, "z") == 0);

	assert_true(strtoi_("5z", &end, 0, INTMAX_MIN, 4, &status) == 4);
	assert_true(status == ERANGE);
	assert_true(strcmp(end, "z") == 0);

	assert_true(errno == 0);
}


static void
test_strtou(void **state)
{
	int   status;
	char  *end;

	errno = 0;
	assert_true(strtou_("42", NULL, -1, 1, 2, &status) == 1);
	assert_true(status == EINVAL);

	assert_true(strtou_("40", &end, 5, 0, UINTMAX_MAX, &status) == 20);
	assert_true(status == 0);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtou_("-40", &end, 0, 0, UINTMAX_MAX, &status) == -40ull);
	assert_true(status == 0);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtou_("z", &end, 0, 0, UINTMAX_MAX, &status) == 0);
	assert_true(status == ECANCELED);
	assert_true(strcmp(end, "z") == 0);

	assert_true(strtou_(" 5", &end, 0, 3, 4, &status) == 4);
	assert_true(status == ERANGE);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtou_("5z", &end, 0, 0, UINTMAX_MAX, &status) == 5);
	assert_true(status == ENOTSUP);
	assert_true(strcmp(end, "z") == 0);

	assert_true(strtou_("5z", &end, 0, 0, 4, &status) == 4);
	assert_true(status == ERANGE);
	assert_true(strcmp(end, "z") == 0);

	assert_true(errno == 0);
}


static void
test_strtou_noneg(void **state)
{
	int   status;
	char  *end;

	errno = 0;
	assert_true(strtou_noneg("42", NULL, -1, 1, 2, &status)
	            == 1);
	assert_true(status == EINVAL);

	assert_true(strtou_noneg("40", &end, 5, 0, UINTMAX_MAX, &status)
	            == 20);
	assert_true(status == 0);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtou_noneg("-40", &end, 0, 2, UINTMAX_MAX, &status)
	            == 2);
	assert_true(status == ERANGE);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtou_noneg("z", &end, 0, 0, UINTMAX_MAX, &status)
	            == 0);
	assert_true(status == ECANCELED);
	assert_true(strcmp(end, "z") == 0);

	assert_true(strtou_noneg(" 5", &end, 0, 3, 4, &status)
	            == 4);
	assert_true(status == ERANGE);
	assert_true(strcmp(end, "") == 0);

	assert_true(strtou_noneg("5z", &end, 0, 0, UINTMAX_MAX, &status)
	            == 5);
	assert_true(status == ENOTSUP);
	assert_true(strcmp(end, "z") == 0);

	assert_true(strtou_noneg("5z", &end, 0, 0, 4, &status)
	            == 4);
	assert_true(status == ERANGE);
	assert_true(strcmp(end, "z") == 0);

	assert_true(errno == 0);
}
