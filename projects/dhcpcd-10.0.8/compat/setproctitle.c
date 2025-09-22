/*
 * Copyright © 2010 William Ahern
 * Copyright © 2012-2013 Guillem Jover <guillem@hadrons.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
//#include "local-link.h"

#include "config.h"

static struct {
	/* Original value. */
	char *arg0;

	/* Title space available. */
	char *base, *end;

	 /* Pointer to original nul character within base. */
	char *nul;

	bool warned;
	bool reset;
	int error;

	/* Our copy of args and environment to free. */
	int argc;
	char **argv;
	char **tmp_environ;
} SPT;


static inline size_t
spt_min(size_t a, size_t b)
{
	return a < b ? a : b;
}

/*
 * For discussion on the portability of the various methods, see
 * https://lists.freebsd.org/pipermail/freebsd-stable/2008-June/043136.html
 */
static int
spt_clearenv(void)
{
#ifdef HAVE_CLEARENV
	return clearenv();
#else
	SPT.tmp_environ = malloc(sizeof(*SPT.tmp_environ));
	if (SPT.tmp_environ == NULL)
		return errno;

	SPT.tmp_environ[0] = NULL;
	environ = SPT.tmp_environ;

	return 0;
#endif
}

static int
spt_copyenv(int envc, char *envp[])
{
	char **envcopy;
	char *eq;
	size_t envsize;
	int i, error;

	if (environ != envp)
		return 0;

	/* Make a copy of the old environ array of pointers, in case
	 * clearenv() or setenv() is implemented to free the internal
	 * environ array, because we will need to access the old environ
	 * contents to make the new copy. */
	envsize = (size_t)(envc + 1) * sizeof(char *);
	envcopy = malloc(envsize);
	if (envcopy == NULL)
		return errno;
	memcpy(envcopy, envp, envsize);

	error = spt_clearenv();
	if (error) {
		environ = envp;
		free(envcopy);
		return error;
	}

	for (i = 0; envcopy[i]; i++) {
		eq = strchr(envcopy[i], '=');
		if (eq == NULL)
			continue;

		*eq = '\0';
		if (setenv(envcopy[i], eq + 1, 1) < 0)
			error = errno;
		*eq = '=';

		if (error) {
#ifdef HAVE_CLEARENV
			/* Because the old environ might not be available
			 * anymore we will make do with the shallow copy. */
			environ = envcopy;
#else
			environ = envp;
			free(envcopy);
#endif
			return error;
		}
	}

	/* Dispose of the shallow copy, now that we've finished transfering
	 * the old environment. */
	free(envcopy);

	return 0;
}

static int
spt_copyargs(int argc, char *argv[])
{
	char *tmp;
	int i;

	for (i = 1; i < argc || (i >= argc && argv[i]); i++) {
		if (argv[i] == NULL)
			continue;

		tmp = strdup(argv[i]);
		if (tmp == NULL)
			return errno;

		argv[i] = tmp;
	}

	return 0;
}

void
setproctitle_init(int argc, char *argv[], char *envp[])
{
	char *base, *end, *nul, *tmp;
	int i, envc, error;

	/* Try to make sure we got called with main() arguments. */
	if (argc < 0)
		return;

	base = argv[0];
	if (base == NULL)
		return;

	nul = &base[strlen(base)];
	end = nul + 1;

	for (i = 0; i < argc || (i >= argc && argv[i]); i++) {
		if (argv[i] == NULL || argv[i] != end)
			continue;

		end = argv[i] + strlen(argv[i]) + 1;
	}

	for (i = 0; envp[i]; i++) {
		if (envp[i] != end)
			continue;

		end = envp[i] + strlen(envp[i]) + 1;
	}
	envc = i;

	SPT.arg0 = strdup(argv[0]);
	if (SPT.arg0 == NULL) {
		SPT.error = errno;
		return;
	}

	tmp = strdup(getprogname());
	if (tmp == NULL) {
		SPT.error = errno;
		return;
	}
	setprogname(tmp);

	error = spt_copyenv(envc, envp);
	if (error) {
		SPT.error = error;
		return;
	}

	error = spt_copyargs(argc, argv);
	if (error) {
		SPT.error = error;
		return;
	}

	SPT.argc = argc;
	SPT.argv = argv;

	SPT.nul  = nul;
	SPT.base = base;
	SPT.end  = end;
}

void
setproctitle_fini(void)
{
	int i;

	free(SPT.arg0);
	SPT.arg0 = NULL;

	for (i = 1; i < SPT.argc; i++) {
		if (SPT.argv[i] != NULL)
			free(SPT.argv[i]);
	}
	SPT.argc = 0;

	free(SPT.tmp_environ);
	SPT.tmp_environ = NULL;
}

#ifndef SPT_MAXTITLE
#define SPT_MAXTITLE 255
#endif

__printflike(1, 2) static void
setproctitle_impl(const char *fmt, ...)
{
	/* Use buffer in case argv[0] is passed. */
	char buf[SPT_MAXTITLE + 1];
	va_list ap;
	char *nul;
	int l;
	size_t len, base_len;

	if (SPT.base == NULL) {
		if (!SPT.warned) {
			warnx("setproctitle not initialized, please either call "
			      "setproctitle_init() or link against libbsd-ctor.");
			SPT.warned = true;
		}
		return;
	}

	if (fmt) {
		if (fmt[0] == '-') {
			/* Skip program name prefix. */
			fmt++;
			len = 0;
		} else {
			/* Print program name heading for grep. */
			l = snprintf(buf, sizeof(buf), "%s: ", getprogname());
			if (l <= 0)
				return;
			len = (size_t)l;
		}

		va_start(ap, fmt);
		l = vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
		va_end(ap);
	} else {
		len = 0;
		l = snprintf(buf, sizeof(buf), "%s", SPT.arg0);
	}

	if (l <= 0) {
		SPT.error = errno;
		return;
	}
	len += (size_t)l;

	base_len = (size_t)(SPT.end - SPT.base);
	if (!SPT.reset) {
		memset(SPT.base, 0, base_len);
		SPT.reset = true;
	} else {
		memset(SPT.base, 0, spt_min(sizeof(buf), base_len));
	}

	len = spt_min(len, spt_min(sizeof(buf), base_len) - 1);
	memcpy(SPT.base, buf, len);
	nul = &SPT.base[len];

	if (nul < SPT.nul) {
		*SPT.nul = '.';
	} else if (nul == SPT.nul && &nul[1] < SPT.end) {
		*SPT.nul = ' ';
		*++nul = '\0';
	}
}
libbsd_symver_default(setproctitle, setproctitle_impl, LIBBSD_0.5);

/* The original function introduced in 0.2 was a stub, it only got implemented
 * in 0.5, make the implementation available in the old version as an alias
 * for code linking against that version, and change the default to use the
 * new version, so that new code depends on the implemented version. */
#ifdef HAVE_TYPEOF
extern __typeof__(setproctitle_impl)
setproctitle_stub
	__attribute__((__alias__("setproctitle_impl")));
#else
void
setproctitle_stub(const char *fmt, ...)
	__attribute__((__alias__("setproctitle_impl")));
#endif
libbsd_symver_variant(setproctitle, setproctitle_stub, LIBBSD_0.2);
