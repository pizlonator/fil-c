/* SPDX-License-Identifier: BSD-2-Clause

  Copyright (c) 2023, Thorsten Kukuk <kukuk@suse.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>
#include <security/_pam_macros.h>

#include "lastlog2.h"
#include "strutils.h"

#define LASTLOG2_DEBUG        01  /* send info to syslog(3) */
#define LASTLOG2_QUIET        02  /* keep quiet about things */

static const char *lastlog2_path = LL2_DEFAULT_DATABASE;

/* check for list match. */
static int
check_in_list (const char *service, const char *arg)
{
	const char *item;
	const char *remaining;

	if (!service)
		return 0;

	remaining = arg;

	for (;;) {
		item = strstr (remaining, service);
		if (item == NULL)
			break;

		/* is it really the start of an item in the list? */
		if (item == arg || *(item - 1) == ',') {
			item += strlen (service);
			/* is item really the service? */
			if (*item == '\0' || *item == ',')
				return 1;
		}

		remaining = strchr (item, ',');
		if (remaining == NULL)
			break;

		/* skip ',' */
		++remaining;
	}

	return 0;
}


static int
_pam_parse_args (pam_handle_t *pamh,
		 int flags, int argc,
		 const char **argv)
{
	int ctrl = 0;
	const char *str;

	/* does the application require quiet? */
	if (flags & PAM_SILENT)
		ctrl |= LASTLOG2_QUIET;

	/* step through arguments */
	for (; argc-- > 0; ++argv) {
		if (strcmp (*argv, "debug") == 0)
			ctrl |= LASTLOG2_DEBUG;
		else if (strcmp (*argv, "silent") == 0)
			ctrl |= LASTLOG2_QUIET;
		else if ((str = startswith (*argv, "database=")) != NULL)
			lastlog2_path = str;
		else if ((str = startswith (*argv, "silent_if=")) != NULL) {
			const void *void_str = NULL;
			const char *service;
			if ((pam_get_item (pamh, PAM_SERVICE, &void_str) != PAM_SUCCESS) ||
			    void_str == NULL)
				service = "";
			else
				service = void_str;

			if (check_in_list (service, str)) {
				if (ctrl & LASTLOG2_DEBUG)
					pam_syslog (pamh, LOG_DEBUG, "silent_if='%s' contains '%s'", str, service);
				ctrl |= LASTLOG2_QUIET;
			}
		} else
			pam_syslog (pamh, LOG_ERR, "Unknown option: %s", *argv);
	}

	return ctrl;
}

static int
write_login_data (pam_handle_t *pamh, int ctrl, const char *user)
{
	const void *void_str;
	const char *tty;
	const char *rhost;
	const char *pam_service;
	const char *xdg_vtnr;
	int xdg_vtnr_nr;
	char tty_buf[8];
	time_t ll_time;
	char *error = NULL;
	int retval;

	void_str = NULL;
	retval = pam_get_item (pamh, PAM_TTY, &void_str);
	if (retval != PAM_SUCCESS || void_str == NULL)
		tty = "";
	else
		tty = void_str;

	/* strip leading "/dev/" from tty. */
	const char *str = startswith(tty, "/dev/");
	if (str != NULL)
		tty = str;

	if (ctrl & LASTLOG2_DEBUG)
		pam_syslog (pamh, LOG_DEBUG, "tty=%s", tty);

	/* if PAM_TTY is not set or an X11 $DISPLAY, try XDG_VTNR */
	if ((tty[0] == '\0' || strchr(tty, ':') != NULL) && (xdg_vtnr = pam_getenv (pamh, "XDG_VTNR")) != NULL) {
		xdg_vtnr_nr = atoi (xdg_vtnr);
		if (xdg_vtnr_nr > 0 && snprintf (tty_buf, sizeof(tty_buf), "tty%d", xdg_vtnr_nr) < (int) sizeof(tty_buf)) {
			tty = tty_buf;
			if (ctrl & LASTLOG2_DEBUG)
				pam_syslog (pamh, LOG_DEBUG, "tty(XDG_VTNR)=%s", tty);
		}
	}

	void_str = NULL;
	retval = pam_get_item (pamh, PAM_RHOST, &void_str);
	if (retval != PAM_SUCCESS || void_str == NULL) {
		void_str = NULL;
		retval = pam_get_item (pamh, PAM_XDISPLAY, &void_str);
		if (retval != PAM_SUCCESS || void_str == NULL) {
			rhost = "";
		} else {
			rhost = void_str;
			if (ctrl & LASTLOG2_DEBUG)
				pam_syslog (pamh, LOG_DEBUG, "rhost(PAM_XDISPLAY)=%s", rhost);
		}
	} else {
		rhost = void_str;
		if (ctrl & LASTLOG2_DEBUG)
			pam_syslog (pamh, LOG_DEBUG, "rhost(PAM_RHOST)=%s", rhost);
	}

	void_str = NULL;
	if ((pam_get_item (pamh, PAM_SERVICE, &void_str) != PAM_SUCCESS) ||
	    void_str == NULL)
		pam_service = "";
	else
		pam_service = void_str;

	if (time (&ll_time) < 0)
		return PAM_SYSTEM_ERR;

	struct ll2_context *context = ll2_new_context(lastlog2_path);
	if (ll2_write_entry (context, user, ll_time, tty, rhost,
			     pam_service, &error) != 0) {
		if (error) {
			pam_syslog (pamh, LOG_ERR, "%s", error);
			free (error);
		} else
			pam_syslog (pamh, LOG_ERR, "Unknown error writing to database %s", lastlog2_path);
		ll2_unref_context(context);
		return PAM_SYSTEM_ERR;
	}
	ll2_unref_context(context);

	return PAM_SUCCESS;
}

static int
show_lastlogin (pam_handle_t *pamh, int ctrl, const char *user)
{
	int64_t ll_time = 0;
	char *tty = NULL;
	char *rhost = NULL;
	char *service = NULL;
	char *date = NULL;
	char the_time[256];
	char *error = NULL;
	int retval = PAM_SUCCESS;

	if (ctrl & LASTLOG2_QUIET)
		return retval;

	struct ll2_context *context = ll2_new_context(lastlog2_path);
	if (ll2_read_entry (context, user, &ll_time, &tty, &rhost,
			    &service, &error) != 0) {
		if (errno == ENOENT)
		{
			/* DB file not found --> it is OK */
			ll2_unref_context(context);
			free(error);
			return PAM_SUCCESS;
		}
		if (error) {
			pam_syslog (pamh, LOG_ERR, "%s", error);
			free (error);
		} else
			pam_syslog (pamh, LOG_ERR, "Unknown error reading database %s", lastlog2_path);
		ll2_unref_context(context);
		return PAM_SYSTEM_ERR;
	}
	ll2_unref_context(context);

	if (ll_time) {
		struct tm *tm, tm_buf;
		/* this is necessary if you compile this on architectures with
		   a 32bit time_t type. */
		time_t t_time = ll_time;		

		if ((tm = localtime_r (&t_time, &tm_buf)) != NULL) {
			strftime (the_time, sizeof (the_time),
				  " %a %b %e %H:%M:%S %Z %Y", tm);
			date = the_time;
		}
	}

	if (date != NULL || rhost != NULL || tty != NULL)
		retval = pam_info(pamh, "Last login:%s%s%s%s%s",
				  date ? date : "",
				  rhost ? " from " : "",
				  rhost ? rhost : "",
				  tty ? " on " : "",
				  tty ? tty : "");

	_pam_drop(service);
	_pam_drop(rhost);
	_pam_drop(tty);

	return retval;
}

int
pam_sm_authenticate (pam_handle_t *pamh __attribute__((__unused__)),
		     int flags __attribute__((__unused__)),
		     int argc __attribute__((__unused__)),
		     const char **argv __attribute__((__unused__)))
{
	return PAM_IGNORE;
}

int
pam_sm_setcred (pam_handle_t *pamh __attribute__((__unused__)),
		int flags __attribute__((__unused__)),
		int argc __attribute__((__unused__)),
		const char **argv __attribute__((__unused__)))
{
	return PAM_IGNORE;
}

int
pam_sm_acct_mgmt (pam_handle_t *pamh __attribute__((__unused__)),
		  int flags __attribute__((__unused__)),
		  int argc __attribute__((__unused__)),
		  const char **argv __attribute__((__unused__)))
{
	return PAM_IGNORE;
}

int
pam_sm_open_session (pam_handle_t *pamh, int flags,
		     int argc, const char **argv)
{
	const struct passwd *pwd;
	const void *void_str;
	const char *user;
	int ctrl;

	ctrl = _pam_parse_args (pamh, flags, argc, argv);

	void_str = NULL;
	int retval = pam_get_item (pamh, PAM_USER, &void_str);
	if (retval != PAM_SUCCESS || void_str == NULL || strlen (void_str) == 0) {
		if (!(ctrl & LASTLOG2_QUIET))
			pam_syslog (pamh, LOG_NOTICE, "User unknown");
		return PAM_USER_UNKNOWN;
	}
	user = void_str;

	/* verify the user exists */
	pwd = pam_modutil_getpwnam (pamh, user);
	if (pwd == NULL) {
		if (ctrl & LASTLOG2_DEBUG)
			pam_syslog (pamh, LOG_DEBUG, "Couldn't find user %s",
				    (const char *)user);
		return PAM_USER_UNKNOWN;
	}

	if (ctrl & LASTLOG2_DEBUG)
		pam_syslog (pamh, LOG_DEBUG, "user=%s", user);

	show_lastlogin (pamh, ctrl, user);

	return write_login_data (pamh, ctrl, user);
}

int
pam_sm_close_session (pam_handle_t *pamh __attribute__((__unused__)),
		      int flags __attribute__((__unused__)),
		      int argc __attribute__((__unused__)),
		      const char **argv __attribute__((__unused__)))
{
	return PAM_SUCCESS;
}
