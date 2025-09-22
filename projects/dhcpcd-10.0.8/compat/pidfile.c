/*	$NetBSD: pidfile.c,v 1.16 2021/08/01 15:29:29 andvar Exp $	*/

/*-
 * Copyright (c) 1999, 2016 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe, Matthias Scheler, Julio Merino and Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <paths.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/file.h>	/* for flock(2) */
#include "config.h"
#include "defs.h"

static pid_t pidfile_pid;
static char pidfile_path[PATH_MAX];
static int pidfile_fd = -1;

/* Closes pidfile resources.
 *
 * Returns 0 on success, otherwise -1. */
static int
pidfile_close(void)
{
	int error;

	pidfile_pid = 0;
	error = close(pidfile_fd);
	pidfile_fd = -1;
	pidfile_path[0] = '\0';
	return error;
}

/* Truncate, close and unlink an existent pidfile,
 * if and only if it was created by this process.
 * The pidfile is truncated because we may have dropped permissions
 * or entered a chroot and thus unable to unlink it.
 *
 * Returns 0 on truncation success, otherwise -1. */
int
pidfile_clean(void)
{
	int error;

	if (pidfile_fd == -1) {
		errno = EBADF;
		return -1;
	}

	if (pidfile_pid != getpid())
		error = EPERM;
	else if (ftruncate(pidfile_fd, 0) == -1)
		error = errno;
	else {
#ifndef HAVE_PLEDGE /* Avoid a pledge violating segfault. */
		(void)unlink(pidfile_path);
#endif
		error = 0;
	}

	(void) pidfile_close();

	if (error != 0) {
		errno = error;
		return -1;
	}
	return 0;
}

/* atexit shim for pidfile_clean */
static void
pidfile_cleanup(void)
{

	pidfile_clean();
}

/* Constructs a name for a pidfile in the default location (/var/run).
 * If 'bname' is NULL, uses the name of the current program for the name of
 * the pidfile.
 *
 * Returns 0 on success, otherwise -1. */
static int
pidfile_varrun_path(char *path, size_t len, const char *bname)
{

	if (bname == NULL)
		bname = PACKAGE;

	/* _PATH_VARRUN includes trailing / */
	if ((size_t)snprintf(path, len, "%s%s.pid", _PATH_VARRUN, bname) >= len)
	{
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

/* Returns the process ID inside path on success, otherwise -1.
 * If no path is given, use the last pidfile path, otherwise the default one. */
pid_t
pidfile_read(const char *path)
{
	char dpath[PATH_MAX], buf[16], *eptr;
	int fd, error;
	ssize_t n;
	pid_t pid;

	if (path == NULL && pidfile_path[0] != '\0')
		path = pidfile_path;
	if (path == NULL || strchr(path, '/') == NULL) {
		if (pidfile_varrun_path(dpath, sizeof(dpath), path) == -1)
			return -1;
		path = dpath;
	}

	if ((fd = open(path, O_RDONLY | O_NONBLOCK)) == -1)
		return  -1;
	n = read(fd, buf, sizeof(buf) - 1);
	error = errno;
	(void) close(fd);
	if (n == -1) {
		errno = error;
		return -1;
	}
	buf[n] = '\0';
	pid = (pid_t)strtoi(buf, &eptr, 10, 1, INT_MAX, &error);
	if (error && !(error == ENOTSUP && *eptr == '\n')) {
		errno = error;
		return -1;
	}
	return pid;
}

/* Locks the pidfile specified by path and writes the process pid to it.
 * The new pidfile is "registered" in the global variables pidfile_fd,
 * pidfile_path and pidfile_pid so that any further call to pidfile_lock(3)
 * can check if we are recreating the same file or a new one.
 *
 * Returns 0 on success, otherwise the pid of the process who owns the
 * lock if it can be read, otherwise -1. */
pid_t
pidfile_lock(const char *path)
{
	char dpath[PATH_MAX];
	static bool registered_atexit = false;

	/* Register for cleanup with atexit. */
	if (!registered_atexit) {
		if (atexit(pidfile_cleanup) == -1)
			return -1;
		registered_atexit = true;
	}

	if (path == NULL || strchr(path, '/') == NULL) {
		if (pidfile_varrun_path(dpath, sizeof(dpath), path) == -1)
			return -1;
		path = dpath;
	}

	/* If path has changed (no good reason), clean up the old pidfile. */
	if (pidfile_fd != -1 && strcmp(pidfile_path, path) != 0)
		pidfile_clean();

	if (pidfile_fd == -1) {
		int fd, opts;

		opts = O_WRONLY | O_CREAT | O_NONBLOCK;
#ifdef O_CLOEXEC
		opts |= O_CLOEXEC;
#endif
#ifdef O_EXLOCK
		opts |=	O_EXLOCK;
#endif
		if ((fd = open(path, opts, 0644)) == -1)
			goto return_pid;
#ifndef O_CLOEXEC
		if ((opts = fcntl(fd, F_GETFD)) == -1 ||
		    fcntl(fd, F_SETFL, opts | FD_CLOEXEC) == -1)
		{
			int error = errno;

			(void) close(fd);
			errno = error;
			return -1;
		}
#endif
#ifndef O_EXLOCK
		if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
			int error = errno;

			(void) close(fd);
			if (error != EAGAIN) {
				errno = error;
				return -1;
			}
			fd = -1;
		}
#endif

return_pid:
		if (fd == -1) {
			pid_t pid;

			if (errno == EAGAIN) {
				/* The pidfile is locked, return the process ID
				 * it contains.
				 * If successful, set errno to EEXIST. */
				if ((pid = pidfile_read(path)) != -1)
					errno = EEXIST;
			} else
				pid = -1;

			return pid;
		}
		pidfile_fd = fd;
		strlcpy(pidfile_path, path, sizeof(pidfile_path));
	}

	pidfile_pid = getpid();

	/* Truncate the file, as we could be re-writing it.
	 * Then write the process ID. */
	if (ftruncate(pidfile_fd, 0) == -1 ||
	    lseek(pidfile_fd, 0, SEEK_SET) == -1 ||
	    dprintf(pidfile_fd, "%d\n", pidfile_pid) == -1)
	{
		int error = errno;

		pidfile_cleanup();
		errno = error;
		return -1;
	}

	/* Hold the fd open to persist the lock. */
	return 0;
}
