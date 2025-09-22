/*
 * Locale-independent strtod().
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * Copyright (C) 2021 Karel Zak <kzak@redhat.com>
 */
#include "c.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "c_strtod.h"

#ifdef __APPLE__
# include <xlocale.h>
#endif

#if defined(HAVE_NEWLOCALE) && (defined(HAVE_STRTOD_L) || defined(HAVE_USELOCALE))
# define USE_CLOCALE
#endif

#if defined(USE_CLOCALE)
static volatile locale_t c_locale;

static locale_t get_c_locale(void)
{
	if (!c_locale)
		c_locale = newlocale(LC_ALL_MASK, "C", (locale_t) 0);
	return c_locale;
}
#endif


double c_strtod(char const *str, char **end)
{
	double res;
	int errsv;

#if defined(USE_CLOCALE)
	locale_t cl = get_c_locale();

#if defined(HAVE_STRTOD_L)
	/*
	 * A) try strtod_l() for "C" locale
	 */
	if (cl)
		return strtod_l(str, end, cl);
#elif defined(HAVE_USELOCALE)
	/*
	 * B) classic strtod(), but switch to "C" locale by uselocale()
	 */
	if (cl) {
		locale_t org_cl = uselocale(cl);
		if (!org_cl)
			return 0;

		res = strtod(str, end);
		errsv = errno;

		uselocale(org_cl);
		errno = errsv;
		return res;
	}
#endif /* HAVE_USELOCALE */
#endif /* USE_CLOCALE */
	/*
	 * C) classic strtod(), but switch to "C" locale by setlocale()
	 */
	char *org_locale = setlocale(LC_NUMERIC, NULL);

	if (org_locale) {
		org_locale = strdup(org_locale);
		if (!org_locale)
			return 0;

		setlocale(LC_NUMERIC, "C");
	}
	res = strtod(str, end);
	errsv = errno;

	if (org_locale) {
		setlocale(LC_NUMERIC, org_locale);
		free(org_locale);
	}
	errno = errsv;
	return res;
}

#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
	double res;
	char *end;

	if (argc < 2) {
		fprintf(stderr, "usage: %s decimal.number\n",
				program_invocation_short_name);
		return EXIT_FAILURE;
	}

	res = c_strtod(argv[1], &end);
	printf("Result: %g, errno: %d, endptr: '%s'\n", res, errno, end);

	return errno ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
