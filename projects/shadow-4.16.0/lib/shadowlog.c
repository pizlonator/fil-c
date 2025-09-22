#include "shadowlog.h"

#include "lib/shadowlog_internal.h"

const char *shadow_progname = "libshadow";
FILE *shadow_logfd = NULL;

void log_set_progname(const char *progname)
{
	shadow_progname = progname;
}

const char *log_get_progname(void)
{
	return shadow_progname;
}

void log_set_logfd(FILE *fd)
{
	if (NULL != fd)
		shadow_logfd = fd;
	else
		shadow_logfd = stderr;
}

FILE *log_get_logfd(void)
{
	if (shadow_logfd != NULL)
		return shadow_logfd;
	return stderr;
}
