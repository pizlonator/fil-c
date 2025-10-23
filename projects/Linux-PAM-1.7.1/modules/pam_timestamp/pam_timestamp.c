/******************************************************************************
 * A module for Linux-PAM that will cache authentication results, inspired by
 * (and implemented with an eye toward being mixable with) sudo.
 *
 * Copyright (c) 2002 Red Hat, Inc.
 * Written by Nalin Dahyabhai <nalin@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <syslog.h>
#include <paths.h>
#ifdef WITH_OPENSSL
#include "hmac_openssl_wrapper.h"
#else
#include "hmacsha1.h"
#endif /* WITH_OPENSSL */

#ifdef USE_LIBSYSTEMD
#include <systemd/sd-login.h>
#elif defined(USE_LIBELOGIND)
#include <elogind/sd-login.h>
#else
#include <utmp.h>
#endif

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>
#include "pam_inline.h"
#include "pam_i18n.h"

/* The default timeout we use is 5 minutes, which matches the sudo default
 * for the timestamp_timeout parameter. */
#define DEFAULT_TIMESTAMP_TIMEOUT (5 * 60)
#define MODULE "pam_timestamp"
#define TIMESTAMPDIR _PATH_VARRUN MODULE
#define TIMESTAMPKEY TIMESTAMPDIR "/_pam_timestamp_key"

/* Various buffers we use need to be at least as large as either PATH_MAX or
 * LINE_MAX, so choose the larger of the two. */
#ifndef PATH_MAX
#define BUFLEN LINE_MAX
#elif (LINE_MAX > PATH_MAX)
#define BUFLEN LINE_MAX
#else
#define BUFLEN PATH_MAX
#endif

#define ROOT_USER 	0
#define ROOT_GROUP 	0

/* Return PAM_SUCCESS if the given directory looks "safe". */
static int
check_dir_perms(pam_handle_t *pamh, const char *tdir)
{
	char scratch[BUFLEN] = {};
	struct stat st;
	size_t i;
	/* Check that the directory is "safe". */
	if ((tdir == NULL) || (strlen(tdir) == 0)) {
		return PAM_AUTH_ERR;
	}
	/* Iterate over the path, checking intermediate directories. */
	for (i = 0; (i < sizeof(scratch)) && (tdir[i] != '\0'); i++) {
		scratch[i] = tdir[i];
		if ((scratch[i] == '/') || (tdir[i + 1] == '\0')) {
			/* We now have the name of a directory in the path, so
			 * we need to check it. */
			if ((lstat(scratch, &st) == -1) && (errno != ENOENT)) {
				pam_syslog(pamh, LOG_ERR,
				       "unable to read `%s': %m",
				       scratch);
				return PAM_AUTH_ERR;
			}
			if (!S_ISDIR(st.st_mode)) {
				pam_syslog(pamh, LOG_ERR,
				       "`%s' is not a directory",
				       scratch);
				return PAM_AUTH_ERR;
			}
			if (S_ISLNK(st.st_mode)) {
				pam_syslog(pamh, LOG_ERR,
				       "`%s' is a symbolic link",
				       scratch);
				return PAM_AUTH_ERR;
			}
			if (st.st_uid != 0) {
				pam_syslog(pamh, LOG_ERR,
				       "`%s' owner UID != 0",
				       scratch);
				return PAM_AUTH_ERR;
			}
			if (st.st_gid != 0) {
				pam_syslog(pamh, LOG_ERR,
				       "`%s' owner GID != 0",
				       scratch);
				return PAM_AUTH_ERR;
			}
			if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
				pam_syslog(pamh, LOG_ERR,
				       "`%s' permissions are lax",
				       scratch);
				return PAM_AUTH_ERR;
			}
		}
	}
	return PAM_SUCCESS;
}

/* Validate a tty pathname as actually belonging to a tty, and return its base
 * name if it's valid. */
static const char *
check_tty(const char *tty)
{
	/* Check that we're not being set up to take a fall. */
	if ((tty == NULL) || (strlen(tty) == 0)) {
		return NULL;
	}
	/* Pull out the meaningful part of the tty's name. */
	if (strchr(tty, '/') != NULL) {
		if (pam_str_skip_prefix(tty, "/dev/") == NULL) {
			/* Make sure the device node is actually in /dev/,
			 * noted by Michal Zalewski. */
			return NULL;
		}
		tty = strrchr(tty, '/') + 1;
	}
	/* Make sure the tty wasn't actually a directory (no basename). */
	if (!strlen(tty) || !strcmp(tty, ".") || !strcmp(tty, "..")) {
		return NULL;
	}
	return tty;
}

/* Determine the right path name for a given user's timestamp. */
static int
format_timestamp_name(char *path, size_t len,
		      const char *timestamp_dir,
		      const char *tty,
		      const char *ruser,
		      const char *user)
{
	if (strcmp(ruser, user) == 0) {
		return pam_snprintf(path, len, "%s/%s/%s", timestamp_dir,
				    ruser, tty);
	} else {
		return pam_snprintf(path, len, "%s/%s/%s:%s", timestamp_dir,
				    ruser, tty, user);
	}
}

/* Check if a given timestamp date, when compared to a current time, fits
 * within the given interval. */
static int
timestamp_good(time_t then, time_t now, time_t interval)
{
	if (((now >= then) && ((now - then) < interval)) ||
	    ((now < then) && ((then - now) < (2 * interval)))) {
		return PAM_SUCCESS;
	}
	return PAM_AUTH_ERR;
}

static int
check_login_time(
#ifdef USE_LOGIND
		 uid_t uid,
#else
		 const char *ruser,
#endif
		 time_t timestamp)
{
	time_t oldest_login = 0;
#ifdef USE_LOGIND
#define USEC_PER_SEC  ((uint64_t) 1000000ULL)
	uint64_t usec = 0;

	if (sd_uid_get_login_time(uid, &usec) < 0) {
	        return PAM_SERVICE_ERR;
	}

	oldest_login = usec/USEC_PER_SEC;
#else
	struct utmp utbuf, *ut;

	setutent();
	while(
#ifdef HAVE_GETUTENT_R
	      !getutent_r(&utbuf, &ut)
#else
	      (ut = getutent()) != NULL
#endif
	      ) {
		if (ut->ut_type != USER_PROCESS) {
			continue;
		}
		if (strncmp(ruser, ut->ut_user, sizeof(ut->ut_user)) != 0) {
			continue;
		}
		if (oldest_login == 0 || oldest_login > ut->ut_tv.tv_sec) {
			oldest_login = ut->ut_tv.tv_sec;
		}
	}
	endutent();
#endif
	if(oldest_login == 0 || timestamp < oldest_login) {
		return PAM_AUTH_ERR;
	}
	return PAM_SUCCESS;
}

#ifndef PAM_TIMESTAMP_MAIN
static int
get_ruser(pam_handle_t *pamh, char *ruserbuf, size_t ruserbuflen)
{
	const void *ruser;
	struct passwd *pwd;

	if (ruserbuf == NULL || ruserbuflen < 1)
		return -2;
	/* Get the name of the source user. */
	if (pam_get_item(pamh, PAM_RUSER, &ruser) != PAM_SUCCESS) {
		ruser = NULL;
	}
	if ((ruser == NULL) || (strlen(ruser) == 0)) {
		/* Barring that, use the current RUID. */
		pwd = pam_modutil_getpwuid(pamh, getuid());
		if (pwd != NULL) {
			ruser = pwd->pw_name;
		}
	} else {
		/*
		 * This ruser is used by format_timestamp_name as a component
		 * of constructed timestamp pathname, so ".", "..", and '/'
		 * are disallowed to avoid potential path traversal issues.
		 */
		if (!strcmp(ruser, ".") ||
		    !strcmp(ruser, "..") ||
		    strchr(ruser, '/')) {
			ruser = NULL;
		}
	}
	if (ruser == NULL || strlen(ruser) >= ruserbuflen) {
		*ruserbuf = '\0';
		return -1;
	}
	strcpy(ruserbuf, ruser);
	return 0;
}

/* Get the path to the timestamp to use. */
static int
get_timestamp_name(pam_handle_t *pamh, int argc, const char **argv,
		   char *path, size_t len)
{
	const char *user, *tty;
	const void *void_tty;
	const char *tdir = TIMESTAMPDIR;
	char ruser[BUFLEN];
	int i, debug = 0;

	/* Parse arguments. */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0) {
			debug = 1;
		}
	}
	for (i = 0; i < argc; i++) {
		const char *str;

		if ((str = pam_str_skip_prefix(argv[i], "timestampdir=")) != NULL) {
			tdir = str;
			if (debug) {
				pam_syslog(pamh, LOG_DEBUG,
				       "storing timestamps in `%s'",
				       tdir);
			}
		}
	}
	i = check_dir_perms(pamh, tdir);
	if (i != PAM_SUCCESS) {
		return i;
	}
	/* Get the name of the target user. */
	if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || user[0] == '\0') {
		return PAM_AUTH_ERR;
	}
	if (debug) {
		pam_syslog(pamh, LOG_DEBUG, "becoming user `%s'", user);
	}
	/* Get the name of the source user. */
	if (get_ruser(pamh, ruser, sizeof(ruser)) || strlen(ruser) == 0) {
		return PAM_AUTH_ERR;
	}
	if (debug) {
		pam_syslog(pamh, LOG_DEBUG, "currently user `%s'", ruser);
	}
	/* Get the name of the terminal. */
	if (pam_get_item(pamh, PAM_TTY, &void_tty) != PAM_SUCCESS) {
		tty = NULL;
	} else {
		tty = void_tty;
	}
	if ((tty == NULL) || (strlen(tty) == 0)) {
		tty = ttyname(STDIN_FILENO);
		if ((tty == NULL) || (strlen(tty) == 0)) {
			tty = ttyname(STDOUT_FILENO);
		}
		if ((tty == NULL) || (strlen(tty) == 0)) {
			tty = ttyname(STDERR_FILENO);
		}
		if ((tty == NULL) || (strlen(tty) == 0)) {
			/* Match sudo's behavior for this case. */
			tty = "unknown";
		}
	}
	if (debug) {
		pam_syslog(pamh, LOG_DEBUG, "tty is `%s'", tty);
	}
	/* Snip off all but the last part of the tty name. */
	tty = check_tty(tty);
	if (tty == NULL) {
		return PAM_AUTH_ERR;
	}
	/* Generate the name of the file used to cache auth results.  These
	 * paths should jive with sudo's per-tty naming scheme. */
	if (format_timestamp_name(path, len, tdir, tty, ruser, user) < 0) {
		return PAM_AUTH_ERR;
	}
	if (debug) {
		pam_syslog(pamh, LOG_DEBUG, "using timestamp file `%s'", path);
	}
	return PAM_SUCCESS;
}

/* Tell the user that access has been granted. */
static void
verbose_success(pam_handle_t *pamh, long diff)
{
	pam_info(pamh, _("Access has been granted"
			 " (last access was %ld seconds ago)."), diff);
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct stat st;
	time_t interval = DEFAULT_TIMESTAMP_TIMEOUT;
	int i, fd, debug = 0, verbose = 0;
	char path[BUFLEN], *p, *message, *message_end;
	long tmp;
	const void *void_service;
	const char *service;
	time_t now, then;

	/* Parse arguments. */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0) {
			debug = 1;
		}
	}
	for (i = 0; i < argc; i++) {
		const char *str;

		if ((str = pam_str_skip_prefix(argv[i], "timestamp_timeout=")) != NULL) {
			tmp = strtol(str, &p, 0);
			if ((p != NULL) && (*p == '\0')) {
				interval = tmp;
				if (debug) {
					pam_syslog(pamh, LOG_DEBUG,
					       "setting timeout to %ld"
					       " seconds", (long)interval);
				}
			}
		} else
		if (strcmp(argv[i], "verbose") == 0) {
			verbose = 1;
			if (debug) {
				pam_syslog(pamh, LOG_DEBUG,
				       "becoming more verbose");
			}
		}
	}

	if (flags & PAM_SILENT) {
		verbose = 0;
	}

	/* Get the name of the timestamp file. */
	if (get_timestamp_name(pamh, argc, argv,
			       path, sizeof(path)) != PAM_SUCCESS) {
		return PAM_AUTH_ERR;
	}

	/* Get the name of the service. */
	if (pam_get_item(pamh, PAM_SERVICE, &void_service) != PAM_SUCCESS) {
		service = NULL;
	} else {
		service = void_service;
	}
	if ((service == NULL) || (strlen(service) == 0)) {
		service = "(unknown)";
	}

	/* Open the timestamp file. */
	fd = open(path, O_RDONLY | O_NOFOLLOW);
	if (fd == -1) {
		if (debug) {
			pam_syslog(pamh, LOG_DEBUG,
			       "cannot open timestamp `%s': %m",
			       path);
		}
		return PAM_AUTH_ERR;
	}

	if (fstat(fd, &st) == 0) {
		int count;
		void *mac;
		size_t maclen;
		char ruser[BUFLEN];

		/* Check that the file is owned by the superuser. */
		if ((st.st_uid != 0) || (st.st_gid != 0)) {
			pam_syslog(pamh, LOG_ERR, "timestamp file `%s' is "
			       "not owned by root", path);
			close(fd);
			return PAM_AUTH_ERR;
		}

		/* Check that the file is a normal file. */
		if (!(S_ISREG(st.st_mode))) {
			pam_syslog(pamh, LOG_ERR, "timestamp file `%s' is "
			       "not a regular file", path);
			close(fd);
			return PAM_AUTH_ERR;
		}

#ifdef WITH_OPENSSL
		if (hmac_size(pamh, debug, &maclen)) {
			close(fd);
			return PAM_AUTH_ERR;
		}
#else
		maclen = hmac_sha1_size();
#endif /* WITH_OPENSSL */
		/* Check that the file is the expected size. */
		if (st.st_size == 0) {
			/* Invalid, but may have been created by sudo. */
			close(fd);
			return PAM_AUTH_ERR;
		}
		if (st.st_size !=
		    (off_t)(strlen(path) + 1 + sizeof(then) + maclen)) {
			pam_syslog(pamh, LOG_NOTICE, "timestamp file `%s' "
			       "appears to be corrupted", path);
			close(fd);
			return PAM_AUTH_ERR;
		}

		/* Read the file contents. */
		message = malloc(st.st_size);
		count = 0;
                if (!message) {
			close(fd);
			return PAM_BUF_ERR;
		}
		while (count < st.st_size) {
			i = read(fd, message + count, st.st_size - count);
			if ((i == 0) || (i == -1)) {
				break;
			}
			count += i;
		}
		if (count < st.st_size) {
			pam_syslog(pamh, LOG_NOTICE, "error reading timestamp "
				"file `%s': %m", path);
			close(fd);
			free(message);
			return PAM_AUTH_ERR;
		}
		message_end = message + strlen(path) + 1 + sizeof(then);

		/* Regenerate the MAC. */
#ifdef WITH_OPENSSL
		if (hmac_generate(pamh, debug, &mac, &maclen, TIMESTAMPKEY,
				  ROOT_USER, ROOT_GROUP, message, message_end - message)) {
			close(fd);
			free(message);
			return PAM_AUTH_ERR;
		}
#else
		hmac_sha1_generate_file(pamh, &mac, &maclen, TIMESTAMPKEY,
					ROOT_USER, ROOT_GROUP, message, message_end - message);
#endif /* WITH_OPENSSL */
		if ((mac == NULL) ||
		    (memcmp(path, message, strlen(path)) != 0) ||
		    (memcmp(mac, message_end, maclen) != 0)) {
			pam_syslog(pamh, LOG_NOTICE, "timestamp file `%s' is "
				"corrupted", path);
			close(fd);
			free(mac);
			free(message);
			return PAM_AUTH_ERR;
		}
		free(mac);
		memmove(&then, message + strlen(path) + 1, sizeof(then));
		free(message);

		/* Check oldest login against timestamp */
		if (get_ruser(pamh, ruser, sizeof(ruser)))
		{
			close(fd);
			return PAM_AUTH_ERR;
		}
#ifdef USE_LOGIND
		struct passwd *pwd = pam_modutil_getpwnam(pamh, ruser);
		if (pwd == NULL) {
		  return PAM_SERVICE_ERR;
		}
		if (check_login_time(pwd->pw_uid, then) != PAM_SUCCESS)
#else
		if (check_login_time(ruser, then) != PAM_SUCCESS)
#endif
		{
			pam_syslog(pamh, LOG_NOTICE, "timestamp file `%s' is "
			       "older than oldest login, disallowing "
			       "access to %s for user %s",
			       path, service, ruser);
			close(fd);
			return PAM_AUTH_ERR;
		}

		/* Compare the dates. */
		now = time(NULL);
		if (timestamp_good(then, now, interval) == PAM_SUCCESS) {
			close(fd);
			pam_syslog(pamh, LOG_NOTICE, "timestamp file `%s' is "
			       "only %ld seconds old, allowing access to %s "
			       "for user %s", path, (long) (now - st.st_mtime),
			       service, ruser);
			if (verbose) {
				verbose_success(pamh, now - st.st_mtime);
			}
			return PAM_SUCCESS;
		} else {
			close(fd);
			pam_syslog(pamh, LOG_NOTICE, "timestamp file `%s' has "
			       "unacceptable age (%ld seconds), disallowing "
			       "access to %s for user %s",
			       path, (long) (now - st.st_mtime),
			       service, ruser);
			return PAM_AUTH_ERR;
		}
	}
	close(fd);

	/* Fail by default. */
	return PAM_AUTH_ERR;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED, int argc UNUSED, const char **argv UNUSED)
{
	return PAM_SUCCESS;
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED, int argc, const char **argv)
{
	char path[BUFLEN], subdir[BUFLEN], *text, *p;
	void *mac;
	size_t maclen;
	time_t now;
	int fd, i, debug = 0;

	/* Parse arguments. */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0) {
			debug = 1;
		}
	}

	/* Get the name of the timestamp file. */
	if (get_timestamp_name(pamh, argc, argv,
			       path, sizeof(path)) != PAM_SUCCESS) {
		return PAM_SESSION_ERR;
	}

	/* Create the directory for the timestamp file if it doesn't already
	 * exist. */
	for (i = 1; i < (int) sizeof(path) && path[i] != '\0'; i++) {
		if (path[i] == '/') {
			/* Attempt to create the directory. */
			memcpy(subdir, path, i);
			subdir[i] = '\0';
			if (mkdir(subdir, 0700) == 0) {
				/* Attempt to set the owner to the superuser. */
			        if (lchown(subdir, 0, 0) != 0) {
					if (debug) {
						pam_syslog(pamh, LOG_DEBUG,
						    "error setting permissions on `%s': %m",
						    subdir);
					}
					return PAM_SESSION_ERR;
				}
			} else {
				if (errno != EEXIST) {
					if (debug) {
						pam_syslog(pamh, LOG_DEBUG,
						    "error creating directory `%s': %m",
						    subdir);
					}
					return PAM_SESSION_ERR;
				}
			}
		}
	}

#ifdef WITH_OPENSSL
	if (hmac_size(pamh, debug, &maclen)) {
		return PAM_SESSION_ERR;
	}
#else
	maclen = hmac_sha1_size();
#endif /* WITH_OPENSSL */

	/* Generate the message. */
	text = malloc(strlen(path) + 1 + sizeof(now) + maclen);
	if (text == NULL) {
		pam_syslog(pamh, LOG_CRIT, "unable to allocate memory: %m");
		return PAM_SESSION_ERR;
	}
	p = text;

	strcpy(text, path);
	p += strlen(path) + 1;

	now = time(NULL);
	memmove(p, &now, sizeof(now));
	p += sizeof(now);

	/* Generate the MAC and append it to the plaintext. */
#ifdef WITH_OPENSSL
	if (hmac_generate(pamh, debug, &mac, &maclen, TIMESTAMPKEY,
			  ROOT_USER, ROOT_GROUP, text, p - text)) {
		free(text);
		return PAM_SESSION_ERR;
	}
#else
	hmac_sha1_generate_file(pamh, &mac, &maclen, TIMESTAMPKEY,
				ROOT_USER, ROOT_GROUP, text, p - text);
	if (mac == NULL) {
		pam_syslog(pamh, LOG_ERR, "failure generating MAC: %m");
		free(text);
		return PAM_SESSION_ERR;
	}
#endif /* WITH_OPENSSL */
	memmove(p, mac, maclen);
	p += maclen;
	free(mac);

	/* Open the file. */
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		pam_syslog(pamh, LOG_ERR, "unable to open `%s': %m", path);
		free(text);
		return PAM_SESSION_ERR;
	}

	/* Attempt to set the owner to the superuser. */
	if (fchown(fd, 0, 0) != 0) {
	  if (debug) {
	    pam_syslog(pamh, LOG_DEBUG,
		       "error setting ownership of `%s': %m",
		       path);
	  }
	  close(fd);
	  free(text);
	  return PAM_SESSION_ERR;
	}


	/* Write the timestamp to the file. */
	if (write(fd, text, p - text) != p - text) {
		pam_syslog(pamh, LOG_ERR, "unable to write to `%s': %m", path);
		close(fd);
		free(text);
		return PAM_SESSION_ERR;
	}

	/* Close the file and return successfully. */
	close(fd);
	free(text);
	pam_syslog(pamh, LOG_DEBUG, "updated timestamp file `%s'", path);
	return PAM_SUCCESS;
}

int
pam_sm_close_session(pam_handle_t *pamh UNUSED, int flags UNUSED, int argc UNUSED, const char **argv UNUSED)
{
	return PAM_SUCCESS;
}

#else /* PAM_TIMESTAMP_MAIN */

#define USAGE "Usage: %s [[-k] | [-d]] [target user]\n"
#define CHECK_INTERVAL 7

int
main(int argc, char **argv)
{
	int i, retval, dflag = 0, kflag = 0;
	const char *target_user = NULL, *user = NULL, *tty = NULL;
	struct passwd *pwd;
	struct timeval tv;
	fd_set write_fds;
	char path[BUFLEN];
	struct stat st;
#ifdef USE_LOGIND
	uid_t uid;
#endif

	/* Check that there's nothing funny going on with stdio. */
	if ((fstat(STDIN_FILENO, &st) == -1) ||
	    (fstat(STDOUT_FILENO, &st) == -1) ||
	    (fstat(STDERR_FILENO, &st) == -1)) {
		/* Appropriate the "no controlling tty" error code. */
		return 3;
	}

	/* Parse arguments. */
	while ((i = getopt(argc, argv, "dk")) != -1) {
		switch (i) {
			case 'd':
				dflag++;
				break;
			case 'k':
				kflag++;
				break;
			default:
				fprintf(stderr, USAGE, argv[0]);
				return 1;
				break;
		}
	}

	/* Bail if both -k and -d are given together. */
	if ((kflag + dflag) > 1) {
		fprintf(stderr, USAGE, argv[0]);
		return 1;
	}

	/* Check that we're setuid. */
	if (geteuid() != 0) {
		fprintf(stderr, "%s must be setuid root\n",
			argv[0]);
		return 2;
	}

	/* Check that we have a controlling tty. */
	tty = ttyname(STDIN_FILENO);
	if ((tty == NULL) || (strlen(tty) == 0)) {
		tty = ttyname(STDOUT_FILENO);
	}
	if ((tty == NULL) || (strlen(tty) == 0)) {
		tty = ttyname(STDERR_FILENO);
	}
	if ((tty == NULL) || (strlen(tty) == 0)) {
		tty = "unknown";
	}

	/* Get the name of the invoking (requesting) user. */
	pwd = getpwuid(getuid());
	if (pwd == NULL) {
		fprintf(stderr, "unknown user\n");
		return 4;
	}
#ifdef USE_LOGIND
	uid = pwd->pw_uid;
#endif

	/* Get the name of the target user. */
	user = strdup(pwd->pw_name);
	if (user == NULL) {
		fprintf(stderr, "out of memory\n");
		return 4;
	}
	target_user = (optind < argc) ? argv[optind] : user;
	if ((strchr(target_user, '.') != NULL) ||
	    (strchr(target_user, '/') != NULL) ||
	    (strchr(target_user, '%') != NULL)) {
		fprintf(stderr, "invalid user: %s\n", target_user);
		return 4;
	}

	/* Sanity check the tty to make sure we should be checking
	 * for timestamps which pertain to it. */
	tty = check_tty(tty);
	if (tty == NULL) {
		fprintf(stderr, "invalid tty\n");
		return 6;
	}

	/* Generate the name of the timestamp file. */
	if (format_timestamp_name(path, sizeof(path), TIMESTAMPDIR,
				  tty, user, target_user) < 0) {
		fprintf(stderr, "path too long\n");
		return 4;
	}

	do {
		retval = 0;
		do {
			/* Sanity check the timestamp directory itself. */
			if (check_dir_perms(NULL, TIMESTAMPDIR) != PAM_SUCCESS) {
				retval = 5;
				break;
			}


			if (kflag) {
				/* Remove the timestamp. */
				if (lstat(path, &st) != -1) {
					retval = unlink(path);
				}
			} else {
				/* Check the timestamp. */
				if (lstat(path, &st) != -1) {
					/* Check oldest login against timestamp */
#ifdef USE_LOGIND
					if (check_login_time(uid, st.st_mtime) != PAM_SUCCESS) {
#else
					if (check_login_time(user, st.st_mtime) != PAM_SUCCESS) {
#endif
						retval = 7;
					} else if (timestamp_good(st.st_mtime, time(NULL),
							DEFAULT_TIMESTAMP_TIMEOUT) != PAM_SUCCESS) {
						retval = 7;
					}
				} else {
					retval = 7;
				}
			}
		} while (0);

		if (dflag > 0) {
			struct timeval now;
			/* Send the would-be-returned value to our parent. */
			signal(SIGPIPE, SIG_DFL);
			fprintf(stdout, "%d\n", retval);
			fflush(stdout);
			/* Wait. */
			gettimeofday(&now, NULL);
			tv.tv_sec = CHECK_INTERVAL;
			/* round the sleep time to get woken up on a whole second */
			tv.tv_usec = 1000000 - now.tv_usec;
			if (now.tv_usec < 500000)
				tv.tv_sec--;
			FD_ZERO(&write_fds);
			FD_SET(STDOUT_FILENO, &write_fds);
			select(STDOUT_FILENO + 1,
			       NULL, NULL, &write_fds,
			       &tv);
		}
	} while (dflag > 0);

	return retval;
}

#endif
