/*
  Copyright (C) 2009  Andreas Gruenbacher <agruen@suse.de>

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Example how to preserve Extended Attributes in file manager style
 * applications. This does NOT also copy Access Control Lists!
 *
 * Andreas Gruenbacher, SuSE Labs, SuSE Linux AG
 * 23 January 2003
 */

#include "config.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <locale.h>
/* #include <libintl.h> */
#ifdef HAVE_ATTR_ERROR_CONTEXT_H
# include <attr/error_context.h>
#endif
#ifdef HAVE_ATTR_LIBATTR_H
# include <attr/libattr.h>
#endif

/*
 * We don't fully internationalize this example!
 */
#define _(msg) (msg)

/*
 * Optional error handler for attr_copy_file(). CTX is the error
 * context passed to attr_copy_file(), ERR is the errno value
 * that occurred. FMT and the rest are printf style arguments.
 */
static void
error(struct error_context *ctx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vfprintf(stderr, fmt, ap))
		fprintf(stderr, ": ");
	fprintf(stderr, "%s\n", strerror(errno));
	va_end(ap);
}

/*
 * Optional handler for quoting path names in error messages.
 * (This is a very stupid example!)
 */
static const char *
quote(struct error_context *ctx, const char *pathname)
{
	char *pn = strdup(pathname), *p;
	pathname = strdup(pathname);
	for (p = pn; *p != '\0'; p++)
		if (*p & 0x80)
			*p='?';
	return pn;
}

static void
quote_free(struct error_context *ctx, const char *name)
{
	free((void *)name);
}

/*
 * The error context we pass to attr_copy_file().
 */
struct error_context ctx = { error, quote, quote_free };

/*
 * Optional attribute filter for attr_copy_file(). This example
 * excludes all attributes other than extended user attributes.
 */
static int is_user_attr(const char *name, struct error_context *ctx)
{
	return strncmp(name, "user.", 5) == 0;
}

int
main(int argc, char *argv[])
{
	int ret;

	/*
	 * Set the locale to enable message translation
	 */
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");

	if (argc != 3) {
		fprintf(stderr, _("Usage: %s from to\n"), argv[0]);
		exit(1);
	}

#if defined(HAVE_ATTR_COPY_FILE)
	/*
	 * If the third parameter is NULL, all extended attributes
	 * except those that define Access Control Lists are copied.
	 * ACLs are excluded by default because copying them between
	 * file systems with and without ACL support needs some
	 * additional logic so that no unexpected permissions result.
	 *
	 * For copying ACLs, please use perm_copy_file() from libacl.
	 *
	 * The CTX parameter could also be NULL, in which case attr_copy_file
	 * would print no error messages.
	 */
	ret = attr_copy_file(argv[1], argv[2], is_user_attr, &ctx);
#else
	fprintf(stderr, _("No support for copying extended attributes\n"));
	ret = -1;
#endif

	if (ret != 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

