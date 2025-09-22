/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * logerr: errx with logging
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/time.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "logerr.h"

#ifndef	LOGERR_SYSLOG_FACILITY
#define	LOGERR_SYSLOG_FACILITY	LOG_DAEMON
#endif

#ifdef SMALL
#undef LOGERR_TAG
#endif

/* syslog protocol is 1k message max, RFC 3164 section 4.1 */
#define LOGERR_SYSLOGBUF	1024 + sizeof(int) + sizeof(pid_t)

#define UNUSED(a)		(void)(a)

struct logctx {
	char		 log_buf[BUFSIZ];
	unsigned int	 log_opts;
	int		 log_fd;
	pid_t		 log_pid;
#ifndef SMALL
	FILE		*log_file;
#ifdef LOGERR_TAG
	const char	*log_tag;
#endif
#endif
};

static struct logctx _logctx = {
	/* syslog style, but without the hostname or tag. */
	.log_opts = LOGERR_LOG | LOGERR_LOG_DATE | LOGERR_LOG_PID,
	.log_fd = -1,
	.log_pid = 0,
};

#if defined(__linux__)
/* Poor man's getprogname(3). */
static char *_logprog;
static const char *
getprogname(void)
{
	const char *p;

	/* Use PATH_MAX + 1 to avoid truncation. */
	if (_logprog == NULL) {
		/* readlink(2) does not append a NULL byte,
		 * so zero the buffer. */
		if ((_logprog = calloc(1, PATH_MAX + 1)) == NULL)
			return NULL;
		if (readlink("/proc/self/exe", _logprog, PATH_MAX + 1) == -1) {
			free(_logprog);
			_logprog = NULL;
			return NULL;
		}
	}
	if (_logprog[0] == '[')
		return NULL;
	p = strrchr(_logprog, '/');
	if (p == NULL)
		return _logprog;
	return p + 1;
}
#endif

#ifndef SMALL
/* Write the time, syslog style. month day time - */
static int
logprintdate(FILE *stream)
{
	struct timeval tv;
	time_t now;
	struct tm tmnow;
	char buf[32];

	if (gettimeofday(&tv, NULL) == -1)
		return -1;

	now = tv.tv_sec;
	if (localtime_r(&now, &tmnow) == NULL)
		return -1;
	if (strftime(buf, sizeof(buf), "%b %d %T ", &tmnow) == 0)
		return -1;
	return fprintf(stream, "%s", buf);
}
#endif

__printflike(3, 0) static int
vlogprintf_r(struct logctx *ctx, FILE *stream, const char *fmt, va_list args)
{
	int len = 0, e;
	va_list a;
#ifndef SMALL
	bool log_pid;
#ifdef LOGERR_TAG
	bool log_tag;
#endif

	if ((stream == stderr && ctx->log_opts & LOGERR_ERR_DATE) ||
	    (stream != stderr && ctx->log_opts & LOGERR_LOG_DATE))
	{
		if ((e = logprintdate(stream)) == -1)
			return -1;
		len += e;
	}

#ifdef LOGERR_TAG
	log_tag = ((stream == stderr && ctx->log_opts & LOGERR_ERR_TAG) ||
	    (stream != stderr && ctx->log_opts & LOGERR_LOG_TAG));
	if (log_tag) {
		if (ctx->log_tag == NULL)
			ctx->log_tag = getprogname();
		if ((e = fprintf(stream, "%s", ctx->log_tag)) == -1)
			return -1;
		len += e;
	}
#endif

	log_pid = ((stream == stderr && ctx->log_opts & LOGERR_ERR_PID) ||
	    (stream != stderr && ctx->log_opts & LOGERR_LOG_PID));
	if (log_pid) {
		pid_t pid;

		if (ctx->log_pid == 0)
			pid = getpid();
		else
			pid = ctx->log_pid;
		if ((e = fprintf(stream, "[%d]", pid)) == -1)
			return -1;
		len += e;
	}

#ifdef LOGERR_TAG
	if (log_tag || log_pid)
#else
	if (log_pid)
#endif
	{
		if ((e = fprintf(stream, ": ")) == -1)
			return -1;
		len += e;
	}
#else
	UNUSED(ctx);
#endif

	va_copy(a, args);
	e = vfprintf(stream, fmt, a);
	if (fputc('\n', stream) == EOF)
		e = -1;
	else if (e != -1)
		e++;
	va_end(a);

	return e == -1 ? -1 : len + e;
}

/*
 * NetBSD's gcc has been modified to check for the non standard %m in printf
 * like functions and warn noisily about it that they should be marked as
 * syslog like instead.
 * This is all well and good, but our logger also goes via vfprintf and
 * when marked as a sysloglike funcion, gcc will then warn us that the
 * function should be printflike instead!
 * This creates an infinte loop of gcc warnings.
 * Until NetBSD solves this issue, we have to disable a gcc diagnostic
 * for our fully standards compliant code in the logger function.
 */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-format-attribute"
#endif
__printflike(2, 0) static int
vlogmessage(int pri, const char *fmt, va_list args)
{
	struct logctx *ctx = &_logctx;
	int len = 0;

	if (ctx->log_fd != -1) {
		char buf[LOGERR_SYSLOGBUF];
		pid_t pid;

		memcpy(buf, &pri, sizeof(pri));
		pid = getpid();
		memcpy(buf + sizeof(pri), &pid, sizeof(pid));
		len = vsnprintf(buf + sizeof(pri) + sizeof(pid),
		    sizeof(buf) - sizeof(pri) - sizeof(pid),
		    fmt, args);
		if (len != -1)
			len = (int)write(ctx->log_fd, buf,
			    ((size_t)++len) + sizeof(pri) + sizeof(pid));
		return len;
	}

	if (ctx->log_opts & LOGERR_ERR &&
	    (pri <= LOG_ERR ||
	    (!(ctx->log_opts & LOGERR_QUIET) && pri <= LOG_INFO) ||
	    (ctx->log_opts & LOGERR_DEBUG && pri <= LOG_DEBUG)))
		len = vlogprintf_r(ctx, stderr, fmt, args);

#ifndef SMALL
	if (ctx->log_file != NULL &&
	    (pri != LOG_DEBUG || (ctx->log_opts & LOGERR_DEBUG)))
		len = vlogprintf_r(ctx, ctx->log_file, fmt, args);
#endif

	if (ctx->log_opts & LOGERR_LOG)
		vsyslog(pri, fmt, args);

	return len;
}
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5))
#pragma GCC diagnostic pop
#endif

__printflike(2, 3) void
logmessage(int pri, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogmessage(pri, fmt, args);
	va_end(args);
}

__printflike(2, 0) static void
vlogerrmessage(int pri, const char *fmt, va_list args)
{
	int _errno = errno;
	char buf[1024];

	vsnprintf(buf, sizeof(buf), fmt, args);
	logmessage(pri, "%s: %s", buf, strerror(_errno));
	errno = _errno;
}

__printflike(2, 3) void
logerrmessage(int pri, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogerrmessage(pri, fmt, args);
	va_end(args);
}

void
log_debug(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogerrmessage(LOG_DEBUG, fmt, args);
	va_end(args);
}

void
log_debugx(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogmessage(LOG_DEBUG, fmt, args);
	va_end(args);
}

void
log_info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogerrmessage(LOG_INFO, fmt, args);
	va_end(args);
}

void
log_infox(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogmessage(LOG_INFO, fmt, args);
	va_end(args);
}

void
log_warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogerrmessage(LOG_WARNING, fmt, args);
	va_end(args);
}

void
log_warnx(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogmessage(LOG_WARNING, fmt, args);
	va_end(args);
}

void
log_err(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogerrmessage(LOG_ERR, fmt, args);
	va_end(args);
}

void
log_errx(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vlogmessage(LOG_ERR, fmt, args);
	va_end(args);
}

int
loggetfd(void)
{
	struct logctx *ctx = &_logctx;

	return ctx->log_fd;
}

void
logsetfd(int fd)
{
	struct logctx *ctx = &_logctx;

	ctx->log_fd = fd;
	if (fd != -1)
		closelog();
#ifndef SMALL
	if (fd != -1 && ctx->log_file != NULL) {
		fclose(ctx->log_file);
		ctx->log_file = NULL;
	}
#endif
}

int
logreadfd(int fd)
{
	struct logctx *ctx = &_logctx;
	char buf[LOGERR_SYSLOGBUF];
	int len, pri;

	len = (int)read(fd, buf, sizeof(buf));
	if (len == -1)
		return -1;

	/* Ensure we have pri, pid and a terminator */
	if (len < (int)(sizeof(pri) + sizeof(pid_t) + 1) ||
	    buf[len - 1] != '\0')
	{
		errno = EINVAL;
		return -1;
	}

	memcpy(&pri, buf, sizeof(pri));
	memcpy(&ctx->log_pid, buf + sizeof(pri), sizeof(ctx->log_pid));
	logmessage(pri, "%s", buf + sizeof(pri) + sizeof(ctx->log_pid));
	ctx->log_pid = 0;
	return len;
}

unsigned int
loggetopts(void)
{
	struct logctx *ctx = &_logctx;

	return ctx->log_opts;
}

void
logsetopts(unsigned int opts)
{
	struct logctx *ctx = &_logctx;

	ctx->log_opts = opts;
	setlogmask(LOG_UPTO(opts & LOGERR_DEBUG ? LOG_DEBUG : LOG_INFO));
}

#ifdef LOGERR_TAG
void
logsettag(const char *tag)
{
#if !defined(SMALL)
	struct logctx *ctx = &_logctx;

	ctx->log_tag = tag;
#else
	UNUSED(tag);
#endif
}
#endif

int
logopen(const char *path)
{
	struct logctx *ctx = &_logctx;
	int opts = 0;

	/* Cache timezone */
	tzset();

	(void)setvbuf(stderr, ctx->log_buf, _IOLBF, sizeof(ctx->log_buf));

#ifndef SMALL
	if (ctx->log_file != NULL) {
		fclose(ctx->log_file);
		ctx->log_file = NULL;
	}
#endif

	if (ctx->log_opts & LOGERR_LOG_PID)
		opts |= LOG_PID;
	openlog(getprogname(), opts, LOGERR_SYSLOG_FACILITY);
	if (path == NULL)
		return 1;

#ifndef SMALL
	if ((ctx->log_file = fopen(path, "ae")) == NULL)
		return -1;
	setlinebuf(ctx->log_file);
	return fileno(ctx->log_file);
#else
	errno = ENOTSUP;
	return -1;
#endif
}

void
logclose(void)
{
#ifndef SMALL
	struct logctx *ctx = &_logctx;
#endif

	closelog();
#if defined(__linux__)
	free(_logprog);
	_logprog = NULL;
#endif
#ifndef SMALL
	if (ctx->log_file == NULL)
		return;
	fclose(ctx->log_file);
	ctx->log_file = NULL;
#endif
}
