/* vi: set sw=4 ts=4: */
/*
 * pid file routines
 *
 * Copyright (C) 2007 by Stephane Billiart <stephane.billiart@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* Override ENABLE_FEATURE_PIDFILE */
#define WANT_PIDFILE 1
#include "libbb.h"

smallint wrote_pidfile;

void FAST_FUNC write_pidfile(const char *path)
{
	int pid_fd;
	char *end;
	char buf[sizeof(int)*3 + 2];
	struct stat sb;

	if (!path)
		return;
	/* we will overwrite stale pidfile */
	pid_fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (pid_fd < 0)
		return;

	/* path can be "/dev/null"! Test for such cases */
	wrote_pidfile = (fstat(pid_fd, &sb) == 0) && S_ISREG(sb.st_mode);

	if (wrote_pidfile) {
		/* few bytes larger, but doesn't use stdio */
		end = utoa_to_buf(getpid(), buf, sizeof(buf));
		*end = '\n';
		full_write(pid_fd, buf, end - buf + 1);
	}
	close(pid_fd);
}

void FAST_FUNC write_pidfile_std_path_and_ext(const char *name)
{
	char buf[sizeof(CONFIG_PID_FILE_PATH) + 64];

	snprintf(buf, sizeof(buf), CONFIG_PID_FILE_PATH"/%s.pid", name);
	write_pidfile(buf);
}

void FAST_FUNC remove_pidfile_std_path_and_ext(const char *name)
{
	char buf[sizeof(CONFIG_PID_FILE_PATH) + 64];

	if (!wrote_pidfile)
		return;
	snprintf(buf, sizeof(buf), CONFIG_PID_FILE_PATH"/%s.pid", name);
	unlink(buf);
}
