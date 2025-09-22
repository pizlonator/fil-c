#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libcommon.h"

const char *progname;

void
set_progname(const char *name)
{
	char *p = strrchr(name, '/');
	progname = (p ? p + 1 : name);
}

const char *
get_progname(void)
{
	return progname;
}

void
print_version_and_exit(void)
{
	printf(_("%s from %s\n"), progname, PACKAGE_STRING);
	exit(0);
}
