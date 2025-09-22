#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "libcommon.h"

extern const char *progname;

void
kbd_warning(const int errnum, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, fmt, ap);

	if (errnum > 0)
		fprintf(stderr, ": %s", strerror(errnum));

	fprintf(stderr, "\n");
	va_end(ap);
	return;
}

void
kbd_error(const int exitnum, const int errnum, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, fmt, ap);

	va_end(ap);

	if (errnum > 0)
		fprintf(stderr, ": %s", strerror(errnum));

	fprintf(stderr, "\n");
	exit(exitnum);
}
