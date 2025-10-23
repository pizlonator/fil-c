/*
 * pam_issue module - a simple /etc/issue parser to set PAM_USER_PROMPT
 *
 * Copyright 1999 by Ben Collins <bcollins@debian.org>
 *
 * Needs to be called before any other auth modules so we can setup the
 * user prompt before it's first used. Allows one argument option, which
 * is the full path to a file to be used for issue (uses /etc/issue as a
 * default) such as "issue=/etc/issue.telnet".
 *
 * We can also parse escapes within the the issue file (enabled by
 * default, but can be disabled with the "noesc" option). It's the exact
 * same parsing as util-linux's agetty program performs.
 *
 * Released under the GNU LGPL version 2 or later
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>
#include <syslog.h>

#ifdef USE_LIBSYSTEMD
#include <systemd/sd-login.h>
#elif defined(USE_LIBELOGIND)
#include <elogind/sd-login.h>
#else
#include <utmp.h>
#endif

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include "pam_inline.h"

static int _user_prompt_set = 0;

static int
read_issue_raw(pam_handle_t *pamh, FILE *fp, char **prompt)
{
    char *issue;
    struct stat st;

    *prompt = NULL;

    if (fstat(fileno(fp), &st) < 0) {
	pam_syslog(pamh, LOG_ERR, "stat error: %m");
	return PAM_SERVICE_ERR;
    }

    if ((issue = malloc(st.st_size + 1)) == NULL) {
	pam_syslog(pamh, LOG_CRIT, "out of memory");
	return PAM_BUF_ERR;
    }

    if ((off_t)fread(issue, 1, st.st_size, fp) != st.st_size) {
	pam_syslog(pamh, LOG_ERR, "read error: %m");
	_pam_drop(issue);
	return PAM_SERVICE_ERR;
    }

    issue[st.st_size] = '\0';
    *prompt = issue;
    return PAM_SUCCESS;
}

static int
read_issue_quoted(pam_handle_t *pamh, FILE *fp, char **prompt)
{
    int c;
    size_t size = 1024;
    size_t issue_len = 0;
    char *issue;
    struct utsname uts;

    *prompt = NULL;

    if ((issue = malloc(size)) == NULL) {
	pam_syslog(pamh, LOG_CRIT, "out of memory");
	return PAM_BUF_ERR;
    }

    (void) uname(&uts);

    while ((c = getc(fp)) != EOF) {
	const char *src = NULL;
	size_t len = 0;
	char buf[1024] = "";

	if (c == '\\') {
	    if ((c = getc(fp)) == EOF)
		break;
	    switch (c) {
	      case 's':
		src = uts.sysname;
		len = strnlen(uts.sysname, sizeof(uts.sysname));
		break;
	      case 'n':
		src = uts.nodename;
		len = strnlen(uts.nodename, sizeof(uts.nodename));
		break;
	      case 'r':
		src = uts.release;
		len = strnlen(uts.release, sizeof(uts.release));
		break;
	      case 'v':
		src = uts.version;
		len = strnlen(uts.version, sizeof(uts.version));
		break;
	      case 'm':
		src = uts.machine;
		len = strnlen(uts.machine, sizeof(uts.machine));
		break;
	      case 'o':
#ifdef HAVE_GETDOMAINNAME
		if (getdomainname(buf, sizeof(buf)) >= 0)
		    buf[sizeof(buf) - 1] = '\0';
#endif
		break;
	      case 'd':
	      case 't':
		{
		    const char *weekday[] = {
			"Sun", "Mon", "Tue", "Wed", "Thu",
			"Fri", "Sat" };
		    const char *month[] = {
			"Jan", "Feb", "Mar", "Apr", "May",
			"Jun", "Jul", "Aug", "Sep", "Oct",
			"Nov", "Dec" };
		    time_t now;
		    struct tm *tm;

		    (void) time (&now);
		    tm = localtime(&now);

		    if (c == 'd')
			pam_sprintf(buf, "%s %s %d  %d",
				    weekday[tm->tm_wday], month[tm->tm_mon],
				    tm->tm_mday, tm->tm_year + 1900);
		    else
			pam_sprintf(buf, "%02d:%02d:%02d",
				    tm->tm_hour, tm->tm_min, tm->tm_sec);
		}
		break;
	      case 'l':
		{
		    const char *ttyn = ttyname(1);
		    if (ttyn) {
			const char *str = pam_str_skip_prefix(ttyn, "/dev/");
			if (str != NULL)
			    ttyn = str;
			src = ttyn;
			len = strlen(ttyn);
		    }
		}
		break;
	      case 'u':
	      case 'U':
		{
		    unsigned int users = 0;
#ifdef USE_LOGIND
		    char **sessions_list;
		    int sessions = sd_get_sessions(&sessions_list);

		    if (sessions < 0) {
		      pam_syslog(pamh, LOG_ERR, "logind error: %s",
				 strerror(-sessions));
		      _pam_drop(issue);
		      return PAM_SERVICE_ERR;
		    } else if (sessions > 0 && sessions_list != NULL) {
		      int i;

		      for (i = 0; i < sessions; i++) {
			char *class;

			if (sd_session_get_class(sessions_list[i], &class) < 0 || class == NULL)
			  continue;

			if (strncmp(class, "user", 4) == 0) // user, user-early, user-incomplete
			  users++;
			free(class);
		      }

		      for (i = 0; i < sessions; i++)
			free(sessions_list[i]);
		      free(sessions_list);
		    } else {
		      users = sessions;
		    }
#else
		    struct utmp *ut;
		    setutent();
		    while ((ut = getutent())) {
			if (ut->ut_type == USER_PROCESS)
			    ++users;
		    }
		    endutent();
#endif
		    if (c == 'U')
			pam_sprintf(buf, "%u %s",
				    users, (users == 1) ? "user" : "users");
		    else
			pam_sprintf(buf, "%u", users);
		    break;
		}
	      default:
		buf[0] = c; buf[1] = '\0';
	    }
	} else {
	    buf[0] = c; buf[1] = '\0';
	}

	if (src == NULL) {
	    src = buf;
	    len = strlen(buf);
	}
	if (issue_len + len + 1 > size) {
	    char *new_issue;

	    size += len + 1;
	    new_issue = realloc (issue, size);
	    if (new_issue == NULL) {
		_pam_drop(issue);
		return PAM_BUF_ERR;
	    }
	    issue = new_issue;
	}
	memcpy(issue + issue_len, src, len);
	issue_len += len;
    }

    issue[issue_len] = '\0';

    if (ferror(fp)) {
	pam_syslog(pamh, LOG_ERR, "read error: %m");
	_pam_drop(issue);
	return PAM_SERVICE_ERR;
    }

    *prompt = issue;
    return PAM_SUCCESS;
}

/* --- authentication management functions (only) --- */

int
pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
    int retval = PAM_SERVICE_ERR;
    FILE *fp;
    const char *issue_file = NULL;
    int parse_esc = 1;
    const void *item = NULL;
    char *issue_prompt = NULL;

   /* If we've already set the prompt, don't set it again */
    if(_user_prompt_set)
	return PAM_IGNORE;

    /* We set this here so if we fail below, we won't get further
       than this next time around (only one real failure) */
    _user_prompt_set = 1;

    for ( ; argc-- > 0 ; ++argv ) {
	const char *str;

	if ((str = pam_str_skip_prefix(*argv, "issue=")) != NULL) {
	    issue_file = str;
	    D(("set issue_file to: %s", issue_file));
	} else if (!strcmp(*argv,"noesc")) {
	    parse_esc = 0;
	    D(("turning off escape parsing by request"));
	} else
	    D(("unknown option passed: %s", *argv));
    }

    if (issue_file == NULL)
	issue_file = "/etc/issue";

    if ((fp = fopen(issue_file, "r")) == NULL) {
	pam_syslog(pamh, LOG_ERR, "error opening %s: %m", issue_file);
	return PAM_SERVICE_ERR;
    }

    if ((retval = pam_get_item(pamh, PAM_USER_PROMPT, &item)) != PAM_SUCCESS) {
	fclose(fp);
	return retval;
    }

    if (parse_esc)
	retval = read_issue_quoted(pamh, fp, &issue_prompt);
    else
	retval = read_issue_raw(pamh, fp, &issue_prompt);

    fclose(fp);

    if (retval != PAM_SUCCESS)
	goto out;

    if (item != NULL) {
	const char *cur_prompt = item;
	char *new_prompt = pam_asprintf("%s%s", issue_prompt, cur_prompt);
	if (new_prompt == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "out of memory");
	    retval = PAM_BUF_ERR;
	    goto out;
	}
	free(issue_prompt);
	issue_prompt = new_prompt;
    }

    retval = pam_set_item(pamh, PAM_USER_PROMPT,
			      (const void *) issue_prompt);
  out:
    _pam_drop(issue_prompt);
    return (retval == PAM_SUCCESS) ? PAM_IGNORE : retval;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
	       int argc UNUSED, const char **argv UNUSED)
{
     return PAM_IGNORE;
}
