/*
 * pam_lastlog module
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/3/11
 *
 * This module does the necessary work to display the last login
 * time+date for this user, it then updates this entry for the
 * present (login) service.
 */

#include "config.h"

#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <utmp.h>
#include <pwd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <syslog.h>
#include <unistd.h>

#if defined(hpux) || defined(sunos) || defined(solaris)
# ifndef _PATH_LASTLOG
#  define _PATH_LASTLOG "/usr/adm/lastlog"
# endif /* _PATH_LASTLOG */
# ifndef UT_HOSTSIZE
#  define UT_HOSTSIZE 16
# endif /* UT_HOSTSIZE */
# ifndef UT_LINESIZE
#  define UT_LINESIZE 12
# endif /* UT_LINESIZE */
#endif
#if defined(hpux)
struct lastlog {
    time_t  ll_time;
    char    ll_line[UT_LINESIZE];
    char    ll_host[UT_HOSTSIZE];            /* same as in utmp */
};
#endif /* hpux */

#ifndef _PATH_BTMP
# define _PATH_BTMP "/var/log/btmp"
#endif

#ifndef PATH_LOGIN_DEFS
# define PATH_LOGIN_DEFS "/etc/login.defs"
#endif

#define DEFAULT_HOST     ""  /* "[no.where]" */
#define DEFAULT_TERM     ""  /* "tt???" */

#define DEFAULT_INACTIVE_DAYS 90
#define MAX_INACTIVE_DAYS 100000
#define LOCK_RETRIES           3  /* number of file lock retries */
#define LOCK_RETRY_DELAY       1  /* seconds to wait between lock attempts */

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_inline.h"
#include "pam_i18n.h"

/* argument parsing */

#define LASTLOG_DATE          01  /* display the date of the last login */
#define LASTLOG_HOST          02  /* display the last host used (if set) */
#define LASTLOG_LINE          04  /* display the last terminal used */
#define LASTLOG_NEVER        010  /* display a welcome message for first login */
#define LASTLOG_DEBUG        020  /* send info to syslog(3) */
#define LASTLOG_QUIET        040  /* keep quiet about things */
#define LASTLOG_WTMP        0100  /* log to wtmp as well as lastlog */
#define LASTLOG_BTMP        0200  /* display failed login info from btmp */
#define LASTLOG_UPDATE      0400  /* update the lastlog and wtmp files (default) */
#define LASTLOG_UNLIMITED  01000  /* unlimited file size (ignore 'fsize' limit) */

static int
_pam_auth_parse(pam_handle_t *pamh, int flags, int argc, const char **argv,
    time_t *inactive)
{
    int ctrl = 0;

    *inactive = DEFAULT_INACTIVE_DAYS;

    /* does the application require quiet? */
    if (flags & PAM_SILENT) {
	ctrl |= LASTLOG_QUIET;
    }

    /* step through arguments */
    for (; argc-- > 0; ++argv) {
        const char *str;
        char *ep = NULL;
        long l;

	if (!strcmp(*argv,"debug")) {
	    ctrl |= LASTLOG_DEBUG;
	} else if (!strcmp(*argv,"silent")) {
	    ctrl |= LASTLOG_QUIET;
	} else if ((str = pam_str_skip_prefix(*argv, "inactive=")) != NULL) {
            l = strtol(str, &ep, 10);
            if (ep != str && l > 0 && l < MAX_INACTIVE_DAYS)
                *inactive = l;
            else {
                pam_syslog(pamh, LOG_ERR, "bad option value: %s", *argv);
            }
	} else {
	    pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
	}
    }

    D(("ctrl = %o", ctrl));
    return ctrl;
}

static int
_pam_session_parse(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    int ctrl=(LASTLOG_DATE|LASTLOG_HOST|LASTLOG_LINE|LASTLOG_WTMP|LASTLOG_UPDATE);

    /* step through arguments */
    for (; argc-- > 0; ++argv) {

	/* generic options */

	if (!strcmp(*argv,"debug")) {
	    ctrl |= LASTLOG_DEBUG;
	} else if (!strcmp(*argv,"nodate")) {
	    ctrl &= ~LASTLOG_DATE;
	} else if (!strcmp(*argv,"noterm")) {
	    ctrl &= ~LASTLOG_LINE;
	} else if (!strcmp(*argv,"nohost")) {
	    ctrl &= ~LASTLOG_HOST;
	} else if (!strcmp(*argv,"silent")) {
	    ctrl |= LASTLOG_QUIET;
	} else if (!strcmp(*argv,"never")) {
	    ctrl |= LASTLOG_NEVER;
	} else if (!strcmp(*argv,"nowtmp")) {
	    ctrl &= ~LASTLOG_WTMP;
	} else if (!strcmp(*argv,"noupdate")) {
	    ctrl &= ~(LASTLOG_WTMP|LASTLOG_UPDATE);
	} else if (!strcmp(*argv,"showfailed")) {
	    ctrl |= LASTLOG_BTMP;
	} else if (!strcmp(*argv,"unlimited")) {
	    ctrl |= LASTLOG_UNLIMITED;
	} else {
	    pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
	}
    }

    /* does the application require quiet? */
    if (flags & PAM_SILENT) {
	ctrl |= LASTLOG_QUIET;
	ctrl &= ~LASTLOG_BTMP;
    }

    D(("ctrl = %o", ctrl));
    return ctrl;
}

static const char *
get_tty(pam_handle_t *pamh)
{
    const void *void_terminal_line = NULL;
    const char *terminal_line;
    const char *str;

    if (pam_get_item(pamh, PAM_TTY, &void_terminal_line) != PAM_SUCCESS
	|| void_terminal_line == NULL) {
	terminal_line = DEFAULT_TERM;
    } else {
	terminal_line = void_terminal_line;
    }

    /* strip leading "/dev/" from tty. */
    str = pam_str_skip_prefix(terminal_line, "/dev/");
    if (str != NULL)
	terminal_line = str;

    D(("terminal = %s", terminal_line));
    return terminal_line;
}

#define MAX_UID_VALUE 0xFFFFFFFFUL

static uid_t
get_lastlog_uid_max(pam_handle_t *pamh)
{
    uid_t uid_max = MAX_UID_VALUE;
    unsigned long ul;
    char *s, *ep;

    s = pam_modutil_search_key(pamh, PATH_LOGIN_DEFS, "LASTLOG_UID_MAX");
    if (s == NULL)
	return uid_max;

    ep = s + strlen(s);
    while (ep > s && isspace((unsigned char)*(--ep))) {
	*ep = '\0';
    }
    errno = 0;
    ul = strtoul(s, &ep, 10);
    if (!(ul >= MAX_UID_VALUE
	|| (uid_t)ul >= MAX_UID_VALUE
	|| (errno != 0 && ul == 0)
	|| s == ep
	|| *ep != '\0')) {
	uid_max = (uid_t)ul;
    }
    free(s);

    return uid_max;
}

static int
last_login_open(pam_handle_t *pamh, int announce, uid_t uid)
{
    int last_fd;

    /* obtain the last login date and all the relevant info */
    last_fd = open(_PATH_LASTLOG, announce&LASTLOG_UPDATE ? O_RDWR : O_RDONLY);
    if (last_fd < 0) {
        if (errno == ENOENT && (announce & LASTLOG_UPDATE)) {
	     last_fd = open(_PATH_LASTLOG, O_RDWR|O_CREAT,
                            S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
             if (last_fd < 0) {
	          pam_syslog(pamh, LOG_ERR,
                             "unable to create %s: %m", _PATH_LASTLOG);
		  D(("unable to create %s file", _PATH_LASTLOG));
		  return -1;
	     }
	     pam_syslog(pamh, LOG_NOTICE,
			"file %s created", _PATH_LASTLOG);
	     D(("file %s created", _PATH_LASTLOG));
	} else {
	  pam_syslog(pamh, LOG_ERR, "unable to open %s: %m", _PATH_LASTLOG);
	  D(("unable to open %s file", _PATH_LASTLOG));
	  return -1;
	}
    }

    if (lseek(last_fd, sizeof(struct lastlog) * (off_t) uid, SEEK_SET) < 0) {
	pam_syslog(pamh, LOG_ERR, "failed to lseek %s: %m", _PATH_LASTLOG);
	D(("unable to lseek %s file", _PATH_LASTLOG));
        close(last_fd);
	return -1;
    }

    return last_fd;
}


static int
last_login_read(pam_handle_t *pamh, int announce, int last_fd, uid_t uid, time_t *lltime)
{
    struct flock last_lock;
    struct lastlog last_login;
    int lock_retries = LOCK_RETRIES;
    int retval = PAM_SUCCESS;
    char the_time[256];
    char *date = NULL;
    char *host = NULL;
    char *line = NULL;

    memset(&last_lock, 0, sizeof(last_lock));
    last_lock.l_type = F_RDLCK;
    last_lock.l_whence = SEEK_SET;
    last_lock.l_start = sizeof(last_login) * (off_t) uid;
    last_lock.l_len = sizeof(last_login);

    while (fcntl(last_fd, F_SETLK, &last_lock) < 0) {
        if (0 == --lock_retries) {
            /* read lock failed, proceed anyway to avoid possible DoS */
            D(("locking %s failed", _PATH_LASTLOG));
            pam_syslog(pamh, LOG_INFO,
                       "file %s is locked/read, proceeding anyway",
                       _PATH_LASTLOG);
            break;
        }
        D(("locking %s failed..(waiting a little)", _PATH_LASTLOG));
        pam_syslog(pamh, LOG_INFO,
                   "file %s is locked/read, retrying", _PATH_LASTLOG);
        sleep(LOCK_RETRY_DELAY);
    }

    if (pam_modutil_read(last_fd, (char *) &last_login,
			 sizeof(last_login)) != sizeof(last_login)) {
        memset(&last_login, 0, sizeof(last_login));
    }

    last_lock.l_type = F_UNLCK;
    (void) fcntl(last_fd, F_SETLK, &last_lock);        /* unlock */

    *lltime = last_login.ll_time;
    if (!last_login.ll_time) {
        if (announce & LASTLOG_DEBUG) {
	    pam_syslog(pamh, LOG_DEBUG,
		       "first login for user with uid %lu",
		       (unsigned long int)uid);
	}
    }

    if (!(announce & LASTLOG_QUIET)) {

	if (last_login.ll_time) {

	    /* we want the date? */
	    if (announce & LASTLOG_DATE) {
	        struct tm *tm, tm_buf;
		time_t ll_time;

		ll_time = last_login.ll_time;
		if ((tm = localtime_r (&ll_time, &tm_buf)) != NULL) {
			strftime (the_time, sizeof (the_time),
		        /* TRANSLATORS: "strftime options for date of last login" */
				  _(" %a %b %e %H:%M:%S %Z %Y"), tm);
			date = the_time;
		}
	    }

	    /* we want & have the host? */
	    if ((announce & LASTLOG_HOST)
		&& (last_login.ll_host[0] != '\0')) {
		/* TRANSLATORS: " from <host>" */
		if ((host = pam_asprintf(_(" from %.*s"), UT_HOSTSIZE,
					 last_login.ll_host)) == NULL) {
		    pam_syslog(pamh, LOG_CRIT, "out of memory");
		    retval = PAM_BUF_ERR;
		    goto cleanup;
		}
	    }

	    /* we want and have the terminal? */
	    if ((announce & LASTLOG_LINE)
		&& (last_login.ll_line[0] != '\0')) {
		/* TRANSLATORS: " on <terminal>" */
		if ((line = pam_asprintf(_(" on %.*s"), UT_LINESIZE,
					 last_login.ll_line)) == NULL) {
		    pam_syslog(pamh, LOG_CRIT, "out of memory");
		    retval = PAM_BUF_ERR;
		    goto cleanup;
		}
	    }

	    if (date != NULL || host != NULL || line != NULL)
		    /* TRANSLATORS: "Last login: <date> from <host> on <terminal>" */
		    retval = pam_info(pamh, _("Last login:%s%s%s"),
			      date ? date : "",
			      host ? host : "",
			      line ? line : "");
	} else if (announce & LASTLOG_NEVER) {
		D(("this is the first time this user has logged in"));
		retval = pam_info(pamh, "%s", _("Welcome to your new account!"));
	}
    }

    /* cleanup */
 cleanup:
    pam_overwrite_object(&last_login);
    pam_overwrite_string(date);
    pam_overwrite_string(host);
    _pam_drop(host);
    pam_overwrite_string(line);
    _pam_drop(line);

    return retval;
}

static int
last_login_write(pam_handle_t *pamh, int announce, int last_fd,
		 uid_t uid, const char *user)
{
    static struct rlimit no_limit = {
	RLIM_INFINITY,
	RLIM_INFINITY
    };
    struct rlimit old_limit;
    int setrlimit_res;
    struct flock last_lock;
    struct lastlog last_login;
    int lock_retries = LOCK_RETRIES;
    time_t ll_time;
    const void *void_remote_host = NULL;
    const char *remote_host;
    const char *terminal_line;
    int retval = PAM_SUCCESS;

    /* rewind */
    if (lseek(last_fd, sizeof(last_login) * (off_t) uid, SEEK_SET) < 0) {
	pam_syslog(pamh, LOG_ERR, "failed to lseek %s: %m", _PATH_LASTLOG);
	return PAM_SERVICE_ERR;
    }

    memset(&last_login, 0, sizeof(last_login));

    /* set this login date */
    D(("set the most recent login time"));
    (void) time(&ll_time);    /* set the time */
    last_login.ll_time = ll_time;

    /* set the remote host */
    if (pam_get_item(pamh, PAM_RHOST, &void_remote_host) != PAM_SUCCESS
	|| void_remote_host == NULL) {
	remote_host = DEFAULT_HOST;
    } else {
	remote_host = void_remote_host;
    }

    /* copy to last_login */
    strncat(last_login.ll_host, remote_host, sizeof(last_login.ll_host)-1);

    /* set the terminal line */
    terminal_line = get_tty(pamh);

    /* copy to last_login */
    strncat(last_login.ll_line, terminal_line, sizeof(last_login.ll_line)-1);
    terminal_line = NULL;

    D(("locking lastlog file"));

    /* now we try to lock this file-record exclusively; non-blocking */
    memset(&last_lock, 0, sizeof(last_lock));
    last_lock.l_type = F_WRLCK;
    last_lock.l_whence = SEEK_SET;
    last_lock.l_start = sizeof(last_login) * (off_t) uid;
    last_lock.l_len = sizeof(last_login);

    while (fcntl(last_fd, F_SETLK, &last_lock) < 0) {
	if (0 == --lock_retries) {
	    D(("locking %s failed", _PATH_LASTLOG));
	    pam_syslog(pamh, LOG_ERR,
		       "file %s is locked/write", _PATH_LASTLOG);
	    return PAM_SERVICE_ERR;
	}
	D(("locking %s failed..(waiting a little)", _PATH_LASTLOG));
	pam_syslog(pamh, LOG_INFO,
		   "file %s is locked/write, retrying", _PATH_LASTLOG);
	sleep(LOCK_RETRY_DELAY);
    }

    /*
     * Failing to set the 'fsize' limit is not a fatal error. We try to write
     * lastlog anyway, under the risk of dying due to a SIGXFSZ.
     */
    D(("setting limit for 'fsize'"));

    if ((announce & LASTLOG_UNLIMITED) == 0) {    /* don't set to unlimited */
	setrlimit_res = -1;
    } else if (getrlimit(RLIMIT_FSIZE, &old_limit) == 0) {
	if (old_limit.rlim_cur == RLIM_INFINITY) {    /* already unlimited */
	    setrlimit_res = -1;
	} else {
	    setrlimit_res = setrlimit(RLIMIT_FSIZE, &no_limit);
	    if (setrlimit_res != 0)
		pam_syslog(pamh, LOG_WARNING, "Could not set limit for 'fsize': %m");
	}
    } else {
	setrlimit_res = -1;
	if (errno == EINVAL) {
	    pam_syslog(pamh, LOG_INFO, "Limit for 'fsize' not supported: %m");
	} else {
	    pam_syslog(pamh, LOG_WARNING, "Could not get limit for 'fsize': %m");
	}
    }

    D(("writing to the lastlog file"));
    if (pam_modutil_write (last_fd, (char *) &last_login,
			   sizeof (last_login)) != sizeof(last_login)) {
	pam_syslog(pamh, LOG_ERR, "failed to write %s: %m", _PATH_LASTLOG);
	retval = PAM_SERVICE_ERR;
    }

    /*
     * Failing to restore the 'fsize' limit is a fatal error.
     */
    D(("restoring limit for 'fsize'"));
    if (setrlimit_res == 0) {
	setrlimit_res = setrlimit(RLIMIT_FSIZE, &old_limit);
	if (setrlimit_res != 0) {
	    pam_syslog(pamh, LOG_ERR, "Could not restore limit for 'fsize': %m");
	    retval = PAM_SERVICE_ERR;
	}
    }

    last_lock.l_type = F_UNLCK;
    (void) fcntl(last_fd, F_SETLK, &last_lock);        /* unlock */
    D(("unlocked"));

    if (announce & LASTLOG_WTMP) {
	/* write wtmp entry for user */
	logwtmp(last_login.ll_line, user, remote_host);
    }

    /* cleanup */
    pam_overwrite_object(&last_login);

    return retval;
}

static int
last_login_date(pam_handle_t *pamh, int announce, uid_t uid, const char *user, time_t *lltime)
{
    int retval;
    int last_fd;

    if (uid > get_lastlog_uid_max(pamh)) {
	return PAM_SUCCESS;
    }

    /* obtain the last login date and all the relevant info */
    last_fd = last_login_open(pamh, announce, uid);
    if (last_fd < 0) {
        return PAM_SERVICE_ERR;
    }

    retval = last_login_read(pamh, announce, last_fd, uid, lltime);
    if (retval != PAM_SUCCESS)
      {
	close(last_fd);
	D(("error while reading lastlog file"));
	return retval;
      }

    if (announce & LASTLOG_UPDATE) {
	retval = last_login_write(pamh, announce, last_fd, uid, user);
    }

    close(last_fd);
    D(("all done with last login"));

    return retval;
}

static int
last_login_failed(pam_handle_t *pamh, int announce, const char *user, time_t lltime)
{
    int retval;
    int fd;
    struct utmp ut;
    struct utmp utuser;
    int failed = 0;
    char the_time[256];
    char *date = NULL;
    char *host = NULL;
    char *line = NULL;

    if (strlen(user) > UT_NAMESIZE) {
	pam_syslog(pamh, LOG_WARNING, "username too long, output might be inaccurate");
    }

    /* obtain the failed login attempt records from btmp */
    fd = open(_PATH_BTMP, O_RDONLY);
    if (fd < 0) {
        int save_errno = errno;
	pam_syslog(pamh, LOG_ERR, "unable to open %s: %m", _PATH_BTMP);
	D(("unable to open %s file", _PATH_BTMP));
        if (save_errno == ENOENT)
	  return PAM_SUCCESS;
	else
	  return PAM_SERVICE_ERR;
    }

    while ((retval=pam_modutil_read(fd, (void *)&ut,
			 sizeof(ut))) == sizeof(ut)) {
	if (ut.ut_tv.tv_sec >= lltime && strncmp(ut.ut_user, user, UT_NAMESIZE) == 0) {
	    memcpy(&utuser, &ut, sizeof(utuser));
	    failed++;
	}
    }

    if (retval != 0)
	pam_syslog(pamh, LOG_ERR, "corruption detected in %s", _PATH_BTMP);
    retval = PAM_SUCCESS;

    if (failed) {
	/* we want the date? */
	if (announce & LASTLOG_DATE) {
	    struct tm *tm, tm_buf;
	    time_t lf_time;

	    lf_time = utuser.ut_tv.tv_sec;
	    if ((tm = localtime_r (&lf_time, &tm_buf)) != NULL) {
	        strftime (the_time, sizeof (the_time),
	            /* TRANSLATORS: "strftime options for date of last login" */
	                  _(" %a %b %e %H:%M:%S %Z %Y"), tm);
	        date = the_time;
	    }
	}

	/* we want & have the host? */
	if ((announce & LASTLOG_HOST)
		&& (utuser.ut_host[0] != '\0')) {
	    /* TRANSLATORS: " from <host>" */
	    if ((host = pam_asprintf(_(" from %.*s"), UT_HOSTSIZE,
				     utuser.ut_host)) == NULL) {
		pam_syslog(pamh, LOG_CRIT, "out of memory");
		retval = PAM_BUF_ERR;
		goto cleanup;
	    }
	}

	/* we want and have the terminal? */
	if ((announce & LASTLOG_LINE)
		&& (utuser.ut_line[0] != '\0')) {
	    /* TRANSLATORS: " on <terminal>" */
	    if ((line = pam_asprintf(_(" on %.*s"), UT_LINESIZE,
				     utuser.ut_line)) == NULL) {
		pam_syslog(pamh, LOG_CRIT, "out of memory");
		retval = PAM_BUF_ERR;
		goto cleanup;
	    }
	}

	if (line != NULL || date != NULL || host != NULL) {
	    /* TRANSLATORS: "Last failed login: <date> from <host> on <terminal>" */
	    pam_info(pamh, _("Last failed login:%s%s%s"),
			      date ? date : "",
			      host ? host : "",
			      line ? line : "");
	}

	_pam_drop(line);
#if defined HAVE_DNGETTEXT && defined ENABLE_NLS
        line = pam_asprintf(dngettext(PACKAGE,
		"There was %d failed login attempt since the last successful login.",
		"There were %d failed login attempts since the last successful login.",
		failed),
	    failed);
#else
	if (failed == 1)
	    line = pam_asprintf(
		_("There was %d failed login attempt since the last successful login."),
		failed);
	else
	    line = pam_asprintf(
		/* TRANSLATORS: only used if dngettext is not supported */
		_("There were %d failed login attempts since the last successful login."),
		failed);
#endif
	if (line != NULL)
		retval = pam_info(pamh, "%s", line);
	else
		retval = PAM_BUF_ERR;
    }

cleanup:
    free(host);
    free(line);
    close(fd);
    D(("all done with btmp"));

    return retval;
}

/* --- authentication (locking out inactive users) functions --- */
static int
pam_auth(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    int retval, ctrl;
    const char *user = NULL;
    const struct passwd *pwd;
    uid_t uid;
    time_t lltime = 0;
    time_t inactive_days = 0;
    int last_fd;

    /*
     * Lock out users if they did not login recently enough.
     */

    ctrl = _pam_auth_parse(pamh, flags, argc, argv, &inactive_days);

    /* which user? */

    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS) {
        pam_syslog(pamh, LOG_NOTICE, "cannot determine user name");
        return PAM_USER_UNKNOWN;
    }

    /* what uid? */

    pwd = pam_modutil_getpwnam (pamh, user);
    if (pwd == NULL) {
        pam_syslog(pamh, LOG_NOTICE, "user unknown");
	return PAM_USER_UNKNOWN;
    }
    uid = pwd->pw_uid;
    pwd = NULL;                                         /* tidy up */

    if (uid == 0 || uid > get_lastlog_uid_max(pamh))
	return PAM_SUCCESS;

    /* obtain the last login date and all the relevant info */
    last_fd = last_login_open(pamh, ctrl, uid);
    if (last_fd < 0) {
	return PAM_IGNORE;
    }

    retval = last_login_read(pamh, ctrl|LASTLOG_QUIET, last_fd, uid, &lltime);
    close(last_fd);

    if (retval != PAM_SUCCESS) {
	D(("error while reading lastlog file"));
	return PAM_IGNORE;
    }

    if (lltime == 0) { /* user never logged in before */
        if (ctrl & LASTLOG_DEBUG)
            pam_syslog(pamh, LOG_DEBUG, "user never logged in - pass");
        return PAM_SUCCESS;
    }

    lltime = (time(NULL) - lltime) / (24*60*60);

    if (lltime > inactive_days) {
        pam_syslog(pamh, LOG_INFO, "user %s inactive for %ld days - denied",
		   user, (long) lltime);
        return PAM_AUTH_ERR;
    }

    return PAM_SUCCESS;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
		    int argc UNUSED, const char **argv UNUSED)
{
    return PAM_SUCCESS;
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags,
		    int argc, const char **argv)
{
    return pam_auth(pamh, flags, argc, argv);
}

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
		    int argc, const char **argv)
{
    return pam_auth(pamh, flags, argc, argv);
}

/* --- session management functions --- */

int
pam_sm_open_session(pam_handle_t *pamh, int flags,
		    int argc, const char **argv)
{
    int retval, ctrl;
    const void *user;
    const struct passwd *pwd;
    uid_t uid;
    time_t lltime = 0;

    /*
     * this module gets the uid of the PAM_USER. Uses it to display
     * last login info and then updates the lastlog for that user.
     */

    ctrl = _pam_session_parse(pamh, flags, argc, argv);

    /* which user? */

    retval = pam_get_item(pamh, PAM_USER, &user);
    if (retval != PAM_SUCCESS || user == NULL || *(const char *)user == '\0') {
	pam_syslog(pamh, LOG_NOTICE, "user unknown");
	return PAM_USER_UNKNOWN;
    }

    /* what uid? */

    pwd = pam_modutil_getpwnam (pamh, user);
    if (pwd == NULL) {
	D(("couldn't identify user %s", (const char *) user));
	return PAM_USER_UNKNOWN;
    }
    uid = pwd->pw_uid;
    pwd = NULL;                                         /* tidy up */

    /* process the current login attempt (indicate last) */

    retval = last_login_date(pamh, ctrl, uid, user, &lltime);

    if ((ctrl & LASTLOG_BTMP) && retval == PAM_SUCCESS) {
	    retval = last_login_failed(pamh, ctrl, user, lltime);
    }

    /* indicate success or failure */

    uid = -1;                                           /* forget this */

    return retval;
}

int
pam_sm_close_session (pam_handle_t *pamh, int flags,
		      int argc, const char **argv)
{
    const char *terminal_line;

    if (!(_pam_session_parse(pamh, flags, argc, argv) & LASTLOG_WTMP))
	return PAM_SUCCESS;

    terminal_line = get_tty(pamh);

    /* Wipe out utmp logout entry */
    logwtmp(terminal_line, "", "");

    return PAM_SUCCESS;
}

/* end of module definition */
