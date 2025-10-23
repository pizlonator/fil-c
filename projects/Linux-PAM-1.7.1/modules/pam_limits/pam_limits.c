/*
 * pam_limits - impose resource limits when opening a user session
 *
 * 1.6 - modified for PLD (added process priority settings)
 *       by Marcin Korzonek <mkorz@shadow.eu.org>
 * 1.5 - Elliot Lee's "max system logins patch"
 * 1.4 - addressed bug in configuration file parser
 * 1.3 - modified the configuration file format
 * 1.2 - added 'debug' and 'conf=' arguments
 * 1.1 - added @group support
 * 1.0 - initial release - Linux ONLY
 *
 * See end for Copyright information
 */

#ifndef __linux__
#warning THIS CODE IS KNOWN TO WORK ONLY ON LINUX !!!
#endif

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <limits.h>
#include <glob.h>
#ifdef USE_LIBSYSTEMD
#include <systemd/sd-login.h>
#elif defined(USE_LIBELOGIND)
#include <elogind/sd-login.h>
#else
#include <utmp.h>
#endif

#ifndef UT_USER  /* some systems have ut_name instead of ut_user */
#define UT_USER ut_user
#endif

#include <grp.h>
#include <pwd.h>
#include <locale.h>

#ifdef HAVE_LIBAUDIT
#include <libaudit.h>
#endif

#ifndef PR_SET_NO_NEW_PRIVS
# define PR_SET_NO_NEW_PRIVS 38 /* from <linux/prctl.h> */
#endif

/* Module defines */
#define LIMITS_DEF_USER     0 /* limit was set by a user entry */
#define LIMITS_DEF_GROUP    1 /* limit was set by a group entry */
#define LIMITS_DEF_ALLGROUP 2 /* limit was set by a group entry */
#define LIMITS_DEF_ALL      3 /* limit was set by an all entry */
#define LIMITS_DEF_DEFAULT  4 /* limit was set by a default entry */
#define LIMITS_DEF_KERNEL   5 /* limit was set from /proc/1/limits */
#define LIMITS_DEF_NONE     6 /* this limit was not set yet */

#define LIMIT_RANGE_ERR  (-1) /* error in specified uid/gid range */
#define LIMIT_RANGE_NONE    0 /* no range specified */
#define LIMIT_RANGE_ONE     1 /* exact uid/gid specified (:max_uid)*/
#define LIMIT_RANGE_MIN     2 /* only minimum uid/gid specified (min_uid:) */
#define LIMIT_RANGE_MM      3 /* both min and max uid/gid specified (min_uid:max_uid) */

static const char *const limits_def_names[] = {
       "USER",
       "GROUP",
       "ALLGROUP",
       "ALL",
       "DEFAULT",
       "KERNEL",
       "NONE",
       NULL
};

struct user_limits_struct {
    int supported;
    int src_soft;
    int src_hard;
    struct rlimit limit;
};

/* internal data */
struct pam_limit_s {
    int login_limit;     /* the max logins limit */
    int login_limit_def; /* which entry set the login limit */
    int flag_numsyslogins; /* whether to limit logins only for a
			      specific user or to count all logins */
    int priority;	 /* the priority to run user process with */
    int nonewprivs;	/* whether to prctl(PR_SET_NO_NEW_PRIVS) */
    struct user_limits_struct limits[RLIM_NLIMITS];
    const char *conf_file;
    int utmp_after_pam_call;
    char *login_group;
};

#define LIMIT_LOGIN (RLIM_NLIMITS+1)
#define LIMIT_NUMSYSLOGINS (RLIM_NLIMITS+2)

#define LIMIT_PRI (RLIM_NLIMITS+3)
#define LIMIT_NONEWPRIVS (RLIM_NLIMITS+4)

#define LIMIT_SOFT  1
#define LIMIT_HARD  2

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_inline.h"
#include "pam_i18n.h"

/* argument parsing */

#define PAM_DEBUG_ARG       0x0001
#define PAM_UTMP_EARLY      0x0004
#define PAM_NO_AUDIT        0x0008
#define PAM_SET_ALL         0x0010

/* Limits from globbed files. */
#define LIMITS_CONF_GLOB	(LIMITS_FILE_DIR "/*.conf")

#define LIMITS_FILE	(SCONFIG_DIR "/limits.conf")

#ifdef VENDOR_SCONFIG_DIR
#define VENDOR_LIMITS_FILE (VENDOR_SCONFIG_DIR "/limits.conf")
#define VENDOR_LIMITS_CONF_GLOB  (VENDOR_SCONFIG_DIR "/limits.d/*.conf")
#endif

static int
_pam_parse (const pam_handle_t *pamh, int argc, const char **argv,
	    struct pam_limit_s *pl)
{
    int ctrl=0;

    /* step through arguments */
    for (ctrl=0; argc-- > 0; ++argv) {
	const char *str;

	/* generic options */

	if (!strcmp(*argv,"debug")) {
	    ctrl |= PAM_DEBUG_ARG;
	} else if ((str = pam_str_skip_prefix(*argv, "conf=")) != NULL) {
	    pl->conf_file = str;
	} else if (!strcmp(*argv,"utmp_early")) {
	    ctrl |= PAM_UTMP_EARLY;
	} else if (!strcmp(*argv,"noaudit")) {
	    ctrl |= PAM_NO_AUDIT;
	} else if (!strcmp(*argv,"set_all")) {
	    ctrl |= PAM_SET_ALL;
	} else {
	    pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
	}
    }

    return ctrl;
}

static const char *
rlimit2str (int i)
{
  switch (i) {
  case RLIMIT_CPU:
    return "cpu";
    break;
  case RLIMIT_FSIZE:
    return "fsize";
    break;
  case RLIMIT_DATA:
    return "data";
    break;
  case RLIMIT_STACK:
    return "stack";
    break;
  case RLIMIT_CORE:
    return "core";
    break;
  case RLIMIT_RSS:
    return "rss";
    break;
  case RLIMIT_NPROC:
    return "nproc";
    break;
  case RLIMIT_NOFILE:
    return "nofile";
    break;
  case RLIMIT_MEMLOCK:
    return "memlock";
    break;
#ifdef RLIMIT_AS
  case RLIMIT_AS:
    return "as";
    break;
#endif
#ifdef RLIMIT_LOCKS
  case RLIMIT_LOCKS:
    return "locks";
    break;
#endif
#ifdef RLIMIT_SIGPENDING
  case RLIMIT_SIGPENDING:
    return "sigpending";
    break;
#endif
#ifdef RLIMIT_MSGQUEUE
  case RLIMIT_MSGQUEUE:
    return "msgqueue";
    break;
#endif
#ifdef RLIMIT_NICE
  case RLIMIT_NICE:
    return "nice";
    break;
#endif
#ifdef RLIMIT_RTPRIO
  case RLIMIT_RTPRIO:
    return "rtprio";
    break;
#endif
#ifdef RLIMIT_RTTIME
  case RLIMIT_RTTIME:
    return "rttime";
    break;
#endif
  default:
    return "UNKNOWN";
    break;
  }
}


#define LIMITED_OK 0 /* limit setting appeared to work */
#define LIMIT_ERR  1 /* error setting a limit */
#define LOGIN_ERR  2 /* too many logins err */

/* Counts the number of user logins and check against the limit*/
static int
check_logins (pam_handle_t *pamh, const char *name, int limit, int ctrl,
              struct pam_limit_s *pl)
{
    int count;

    if (ctrl & PAM_DEBUG_ARG) {
        pam_syslog(pamh, LOG_DEBUG,
		   "checking logins for '%s' (maximum of %d)", name, limit);
    }

    if (limit < 0)
        return 0; /* no limits imposed */
    if (limit == 0) /* maximum 0 logins ? */ {
        pam_syslog(pamh, LOG_WARNING, "No logins allowed for '%s'", name);
        return LOGIN_ERR;
    }

    /* Because there is no definition about when an application
       actually adds a utmp entry, some applications bizarrely do the
       utmp call before the have PAM authenticate them to the system:
       you're logged it, sort of...? Anyway, you can use the
       "utmp_early" module argument in your PAM config file to make
       allowances for this sort of problem. (There should be a PAM
       standard for this, since if a module wants to actually map a
       username then any early utmp entry will be for the unmapped
       name = broken.) */

    if (ctrl & PAM_UTMP_EARLY) {
	count = 0;
    } else {
	count = 1;
    }

#ifdef USE_LOGIND
    char **sessions_list;
    int sessions = sd_get_sessions(&sessions_list);

    /* maxlogins needs to be 2 with systemd-logind because
       of the systemd --user process started with first login by
       pam_systemd.
       Which is also calling pam_limits, but in this very first special
       case the session does already exist and is counted twice.
       With start of the second session, session manager is already running
       and no longer counted. */
    if (limit == 1) {
        pam_syslog(pamh, LOG_WARNING, "Maxlogin limit needs to be 2 or higher with systemd-logind");
        return LIMIT_ERR;
    }

    if (sessions < 0) {
      pam_syslog(pamh, LOG_ERR, "logind error getting session list: %s",
		 strerror(-sessions));
      return LIMIT_ERR;
    } else if (sessions > 0 && sessions_list != NULL && !pl->flag_numsyslogins) {
      int i;

      for (i = 0; i < sessions; i++) {
	char *user = NULL;
	char *class = NULL;

	if (sd_session_get_class(sessions_list[i], &class) < 0 || class == NULL)
	  continue;

	if (strncmp(class, "user", 4) != 0)  { /* user, user-early, user-incomplete */
	  free (class);
	  continue;
	}
	free (class);

	if (sd_session_get_username(sessions_list[i], &user) < 0 || user == NULL) {
	  pam_syslog(pamh, LOG_ERR, "logind error getting username: %s",
		     strerror(-sessions));
	  return LIMIT_ERR;
	}

	if (((pl->login_limit_def == LIMITS_DEF_USER)
	     || (pl->login_limit_def == LIMITS_DEF_GROUP)
	     || (pl->login_limit_def == LIMITS_DEF_DEFAULT))
	    && strcmp(name, user) != 0) {
	  free(user);
	  continue;
	}
	if ((pl->login_limit_def == LIMITS_DEF_ALLGROUP)
	    && pl->login_group != NULL
	    && !pam_modutil_user_in_group_nam_nam(pamh, user, pl->login_group)) {
	  free(user);
	  continue;
	}
	free(user);

	if (++count > limit) {
	  break;
	}
      }
      for (i = 0; i < sessions; i++)
	free(sessions_list[i]);
      free(sessions_list);
    } else {
      count = sessions;
    }
#else
    struct utmp *ut;

    setutent();

    while((ut = getutent())) {
#ifdef USER_PROCESS
        if (ut->ut_type != USER_PROCESS) {
            continue;
	}
#endif
        if (ut->UT_USER[0] == '\0') {
            continue;
	}
        if (!pl->flag_numsyslogins) {
	    char user[sizeof(ut->UT_USER) + 1];
	    memcpy(user, ut->UT_USER, sizeof(ut->UT_USER));
	    user[sizeof(ut->UT_USER)] = '\0';

	    if (((pl->login_limit_def == LIMITS_DEF_USER)
	         || (pl->login_limit_def == LIMITS_DEF_GROUP)
		 || (pl->login_limit_def == LIMITS_DEF_DEFAULT))
		&& strcmp(name, user) != 0) {
                continue;
	    }
	    if ((pl->login_limit_def == LIMITS_DEF_ALLGROUP)
		&& pl->login_group != NULL
		&& !pam_modutil_user_in_group_nam_nam(pamh, user, pl->login_group)) {
                continue;
	    }
	    if (kill(ut->ut_pid, 0) == -1 && errno == ESRCH) {
		/* process does not exist anymore */
		pam_syslog(pamh, LOG_INFO,
			   "Stale utmp entry (pid %d) for '%s' ignored",
			   ut->ut_pid, user);
		continue;
	    }
	}
	if (++count > limit) {
	    break;
	}
    }
    endutent();
#endif
    if (count > limit) {
	if (name) {
	    pam_syslog(pamh, LOG_NOTICE,
		       "Too many logins (max %d) for %s", limit, name);
	} else {
	    pam_syslog(pamh, LOG_NOTICE, "Too many system logins (max %d)", limit);
	}
        return LOGIN_ERR;
    }
    return 0;
}

#ifdef __linux__
static const char *const lnames[RLIM_NLIMITS] = {
        [RLIMIT_CPU] = "Max cpu time",
        [RLIMIT_FSIZE] = "Max file size",
        [RLIMIT_DATA] = "Max data size",
        [RLIMIT_STACK] = "Max stack size",
        [RLIMIT_CORE] = "Max core file size",
        [RLIMIT_RSS] = "Max resident set",
        [RLIMIT_NPROC] = "Max processes",
        [RLIMIT_NOFILE] = "Max open files",
        [RLIMIT_MEMLOCK] = "Max locked memory",
#ifdef RLIMIT_AS
        [RLIMIT_AS] = "Max address space",
#endif
#ifdef RLIMIT_LOCKS
        [RLIMIT_LOCKS] = "Max file locks",
#endif
#ifdef RLIMIT_SIGPENDING
        [RLIMIT_SIGPENDING] = "Max pending signals",
#endif
#ifdef RLIMIT_MSGQUEUE
        [RLIMIT_MSGQUEUE] = "Max msgqueue size",
#endif
#ifdef RLIMIT_NICE
        [RLIMIT_NICE] = "Max nice priority",
#endif
#ifdef RLIMIT_RTPRIO
        [RLIMIT_RTPRIO] = "Max realtime priority",
#endif
#ifdef RLIMIT_RTTIME
        [RLIMIT_RTTIME] = "Max realtime timeout",
#endif
};

static int str2rlimit(char *name) {
    int i;
    if (!name || *name == '\0')
        return -1;
    for(i = 0; i < RLIM_NLIMITS; i++) {
        if (strcmp(name, lnames[i]) == 0) return i;
    }
    return -1;
}

static rlim_t str2rlim_t(char *value) {
    unsigned long long rlimit = 0;

    if (!value) return (rlim_t)rlimit;
    if (strcmp(value, "unlimited") == 0) {
        return RLIM_INFINITY;
    }
    rlimit = strtoull(value, NULL, 10);
    return (rlim_t)rlimit;
}

#define LIMITS_SKIP_WHITESPACE { \
        /* step backwards over spaces */ \
        pos--; \
        while (pos && line[pos] == ' ') pos--; \
        if (!pos) continue; \
        line[pos+1] = '\0'; \
}
#define LIMITS_MARK_ITEM(item) { \
        /* step backwards over non-spaces */ \
        pos--; \
        while (pos && line[pos] != ' ') pos--; \
        if (!pos) continue; \
        (item) = line + pos + 1; \
}

static void parse_kernel_limits(pam_handle_t *pamh, struct pam_limit_s *pl, int ctrl)
{
    int i;
    FILE *limitsfile;
    const char *proclimits = "/proc/1/limits";
    char *line = NULL;
    size_t maxlen = 0, n = 0;
    char *hard, *soft, *name;

    if (!(limitsfile = fopen(proclimits, "r"))) {
        pam_syslog(pamh, LOG_WARNING, "Could not read %s (%s), using PAM defaults", proclimits, strerror(errno));
        return;
    }

    while (getline(&line, &n, limitsfile) != -1) {
        size_t pos = strlen(line);
        if (pos < 2) continue;

        /* drop trailing newline */
        if (line[pos-1] == '\n') {
            pos--;
            line[pos] = '\0';
        }

        /* determine formatting boundary of limits report */
        if (!maxlen && pam_str_skip_prefix(line, "Limit") != NULL) {
            maxlen = pos;
            continue;
        }

        if (pos == maxlen) {
            /* step backwards over "Units" name */
            LIMITS_SKIP_WHITESPACE;
            LIMITS_MARK_ITEM(hard); /* not a typo, units unused */
        }

        /* step backwards over "Hard Limit" value */
        LIMITS_SKIP_WHITESPACE;
        LIMITS_MARK_ITEM(hard);

        /* step backwards over "Soft Limit" value */
        LIMITS_SKIP_WHITESPACE;
        LIMITS_MARK_ITEM(soft);

        /* step backwards over name of limit */
        LIMITS_SKIP_WHITESPACE;
        name = line;

        i = str2rlimit(name);
        if (i < 0 || i >= RLIM_NLIMITS) {
            if (ctrl & PAM_DEBUG_ARG)
                pam_syslog(pamh, LOG_DEBUG, "Unknown kernel rlimit '%s' ignored", name);
            continue;
        }
        pl->limits[i].limit.rlim_cur = str2rlim_t(soft);
        pl->limits[i].limit.rlim_max = str2rlim_t(hard);
        pl->limits[i].src_soft = LIMITS_DEF_KERNEL;
        pl->limits[i].src_hard = LIMITS_DEF_KERNEL;
    }
    free(line);
    fclose(limitsfile);
}
#endif

static int init_limits(pam_handle_t *pamh, struct pam_limit_s *pl, int ctrl)
{
    int i;
    int retval = PAM_SUCCESS;

    D(("called."));

    for(i = 0; i < RLIM_NLIMITS; i++) {
	int r = getrlimit(i, &pl->limits[i].limit);
	if (r == -1) {
	    pl->limits[i].supported = 0;
	    if (errno != EINVAL) {
		retval = !PAM_SUCCESS;
	    }
	} else {
	    pl->limits[i].supported = 1;
	    pl->limits[i].src_soft = LIMITS_DEF_NONE;
	    pl->limits[i].src_hard = LIMITS_DEF_NONE;
	}
    }

#ifdef __linux__
    if (ctrl & PAM_SET_ALL) {
      parse_kernel_limits(pamh, pl, ctrl);

      for(i = 0; i < RLIM_NLIMITS; i++) {
	if (pl->limits[i].supported &&
	    (pl->limits[i].src_soft == LIMITS_DEF_NONE ||
	     pl->limits[i].src_hard == LIMITS_DEF_NONE)) {
	  pam_syslog(pamh, LOG_WARNING, "Did not find kernel RLIMIT for %s, using PAM default", rlimit2str(i));
	}
      }
    }
#endif

    errno = 0;
    pl->priority = getpriority (PRIO_PROCESS, 0);
    if (pl->priority == -1 && errno != 0)
      retval = !PAM_SUCCESS;
    pl->login_limit = -2;
    pl->login_limit_def = LIMITS_DEF_NONE;
    pl->login_group = NULL;

    return retval;
}

/*
 * Read the contents of <pathname> and return it in *valuep
 * return 1 if conversion succeeds, result is in *valuep
 * return 0 if conversion fails, *valuep is untouched.
 */
static int
value_from_file(const char *pathname, rlim_t *valuep)
{
    FILE *fp;
    int retval;

    retval = 0;

    if ((fp = fopen(pathname, "r")) != NULL) {
	char *buf = NULL;
	size_t n = 0;

	if (getline(&buf, &n, fp) != -1) {
	    char *endptr;
	    unsigned long long value;

	    errno = 0;
	    value = strtoull(buf, &endptr, 10);
	    if (endptr != buf &&
		(value != ULLONG_MAX || errno == 0) &&
                (unsigned long long) (rlim_t) value == value) {
		*valuep = (rlim_t) value;
		retval = 1;
	    }
	}

	free(buf);
	fclose(fp);
    }

    return retval;
}

static void
process_limit (const pam_handle_t *pamh, int source, const char *lim_type,
	       const char *lim_item, const char *lim_value,
	       int ctrl, struct pam_limit_s *pl)
{
    int limit_item;
    int limit_type = 0;
    int int_value = 0;
    rlim_t rlimit_value = 0;
    char *endptr;
    const char *value_orig = lim_value;

    if (ctrl & PAM_DEBUG_ARG)
	 pam_syslog(pamh, LOG_DEBUG, "%s: processing %s %s %s for %s",
		    __FUNCTION__, lim_type, lim_item, lim_value,
		    limits_def_names[source]);

    if (strcmp(lim_item, "cpu") == 0)
        limit_item = RLIMIT_CPU;
    else if (strcmp(lim_item, "fsize") == 0)
        limit_item = RLIMIT_FSIZE;
    else if (strcmp(lim_item, "data") == 0)
	limit_item = RLIMIT_DATA;
    else if (strcmp(lim_item, "stack") == 0)
	limit_item = RLIMIT_STACK;
    else if (strcmp(lim_item, "core") == 0)
	limit_item = RLIMIT_CORE;
    else if (strcmp(lim_item, "rss") == 0)
	limit_item = RLIMIT_RSS;
    else if (strcmp(lim_item, "nproc") == 0)
	limit_item = RLIMIT_NPROC;
    else if (strcmp(lim_item, "nofile") == 0)
	limit_item = RLIMIT_NOFILE;
    else if (strcmp(lim_item, "memlock") == 0)
	limit_item = RLIMIT_MEMLOCK;
#ifdef RLIMIT_AS
    else if (strcmp(lim_item, "as") == 0)
	limit_item = RLIMIT_AS;
#endif /*RLIMIT_AS*/
#ifdef RLIMIT_LOCKS
    else if (strcmp(lim_item, "locks") == 0)
	limit_item = RLIMIT_LOCKS;
#endif
#ifdef RLIMIT_SIGPENDING
    else if (strcmp(lim_item, "sigpending") == 0)
	limit_item = RLIMIT_SIGPENDING;
#endif
#ifdef RLIMIT_MSGQUEUE
    else if (strcmp(lim_item, "msgqueue") == 0)
	limit_item = RLIMIT_MSGQUEUE;
#endif
#ifdef RLIMIT_NICE
    else if (strcmp(lim_item, "nice") == 0)
	limit_item = RLIMIT_NICE;
#endif
#ifdef RLIMIT_RTPRIO
    else if (strcmp(lim_item, "rtprio") == 0)
	limit_item = RLIMIT_RTPRIO;
#endif
#ifdef RLIMIT_RTTIME
    else if (strcmp(lim_item, "rttime") == 0)
	limit_item = RLIMIT_RTTIME;
#endif
    else if (strcmp(lim_item, "maxlogins") == 0) {
	limit_item = LIMIT_LOGIN;
	pl->flag_numsyslogins = 0;
    } else if (strcmp(lim_item, "maxsyslogins") == 0) {
	limit_item = LIMIT_NUMSYSLOGINS;
	pl->flag_numsyslogins = 1;
    } else if (strcmp(lim_item, "priority") == 0) {
	limit_item = LIMIT_PRI;
    } else if (strcmp(lim_item, "nonewprivs") == 0) {
	limit_item = LIMIT_NONEWPRIVS;
    } else {
        pam_syslog(pamh, LOG_DEBUG, "unknown limit item '%s'", lim_item);
        return;
    }

    if (strcmp(lim_type,"soft")==0)
	limit_type=LIMIT_SOFT;
    else if (strcmp(lim_type, "hard")==0)
	limit_type=LIMIT_HARD;
    else if (strcmp(lim_type,"-")==0)
	limit_type=LIMIT_SOFT | LIMIT_HARD;
    else if (limit_item != LIMIT_LOGIN && limit_item != LIMIT_NUMSYSLOGINS
		&& limit_item != LIMIT_NONEWPRIVS) {
        pam_syslog(pamh, LOG_DEBUG, "unknown limit type '%s'", lim_type);
        return;
    }
	if (limit_item == LIMIT_NONEWPRIVS) {
		/* just require a bool-style 0 or 1 */
		if (strcmp(lim_value, "0") == 0) {
			int_value = 0;
		} else if (strcmp(lim_value, "1") == 0) {
			int_value = 1;
		} else {
			pam_syslog(pamh, LOG_DEBUG,
				   "wrong limit value '%s' for limit type '%s'",
				   lim_value, lim_type);
		}
	} else if (limit_item != LIMIT_PRI
#ifdef RLIMIT_NICE
	    && limit_item != RLIMIT_NICE
#endif
	    && (strcmp(lim_value, "-1") == 0
		|| strcmp(lim_value, "-") == 0 || strcmp(lim_value, "unlimited") == 0
		|| strcmp(lim_value, "infinity") == 0)) {
		int_value = -1;
		rlimit_value = RLIM_INFINITY;
	} else if (limit_item == LIMIT_PRI || limit_item == LIMIT_LOGIN ||
#ifdef RLIMIT_NICE
		limit_item == RLIMIT_NICE ||
#endif
		limit_item == LIMIT_NUMSYSLOGINS) {
		long temp;
		temp = strtol (lim_value, &endptr, 10);
		temp = temp < INT_MAX ? temp : INT_MAX;
		int_value = temp > INT_MIN ? temp : INT_MIN;
		if (int_value == 0 && value_orig == endptr) {
			pam_syslog(pamh, LOG_DEBUG,
				   "wrong limit value '%s' for limit type '%s'",
				   lim_value, lim_type);
            return;
		}
	} else {
#ifdef __USE_FILE_OFFSET64
		rlimit_value = strtoull (lim_value, &endptr, 10);
#else
		rlimit_value = strtoul (lim_value, &endptr, 10);
#endif
		if (rlimit_value == 0 && value_orig == endptr) {
			pam_syslog(pamh, LOG_DEBUG,
				   "wrong limit value '%s' for limit type '%s'",
				   lim_value, lim_type);
			return;
		}
	}

    /* one more special case when limiting logins */
    if ((source == LIMITS_DEF_ALL || source == LIMITS_DEF_ALLGROUP)
		&& (limit_item != LIMIT_LOGIN)) {
	if (ctrl & PAM_DEBUG_ARG)
	    pam_syslog(pamh, LOG_DEBUG,
		       "'%%' domain valid for maxlogins type only");
	return;
    }

    switch(limit_item) {
        case RLIMIT_CPU:
	  if (rlimit_value != RLIM_INFINITY)
	    {
	      if (rlimit_value >= RLIM_INFINITY/60)
		rlimit_value = RLIM_INFINITY;
	      else
		rlimit_value *= 60;
	    }
         break;
        case RLIMIT_FSIZE:
        case RLIMIT_DATA:
        case RLIMIT_STACK:
        case RLIMIT_CORE:
        case RLIMIT_RSS:
        case RLIMIT_MEMLOCK:
#ifdef RLIMIT_AS
        case RLIMIT_AS:
#endif
         if (rlimit_value != RLIM_INFINITY)
	   {
	     if (rlimit_value >= RLIM_INFINITY/1024)
	       rlimit_value = RLIM_INFINITY;
	     else
	       rlimit_value *= 1024;
	   }
	 break;
#ifdef RLIMIT_NICE
	case RLIMIT_NICE:
	 if (int_value > 19)
	    int_value = 19;
	 if (int_value < -20)
	   int_value = -20;
	 rlimit_value = 20 - int_value;
         break;
#endif
	case RLIMIT_NOFILE:
	/*
	 * If nofile is to be set to "unlimited", try to set it to
	 * the value in /proc/sys/fs/nr_open instead.
	 */
	if (rlimit_value == RLIM_INFINITY) {
	    if (!value_from_file("/proc/sys/fs/nr_open", &rlimit_value))
		pam_syslog(pamh, LOG_WARNING,
			   "Cannot set \"nofile\" to a sensible value");
	    else if (ctrl & PAM_DEBUG_ARG)
		pam_syslog(pamh, LOG_DEBUG, "Setting \"nofile\" limit to %llu",
			   (unsigned long long) rlimit_value);
	}
	break;
    }

    if ( (limit_item != LIMIT_LOGIN)
	 && (limit_item != LIMIT_NUMSYSLOGINS)
	 && (limit_item != LIMIT_PRI)
	 && (limit_item != LIMIT_NONEWPRIVS) ) {
        if (limit_type & LIMIT_SOFT) {
	    if (pl->limits[limit_item].src_soft < source) {
                return;
	    } else {
                pl->limits[limit_item].limit.rlim_cur = rlimit_value;
                pl->limits[limit_item].src_soft = source;
            }
	}
        if (limit_type & LIMIT_HARD) {
	    if (pl->limits[limit_item].src_hard < source) {
                return;
            } else {
                pl->limits[limit_item].limit.rlim_max = rlimit_value;
                pl->limits[limit_item].src_hard = source;
            }
	}
    } else {
	/* recent kernels support negative priority limits (=raise priority) */

	if (limit_item == LIMIT_PRI) {
	    pl->priority = int_value;
	} else if (limit_item == LIMIT_NONEWPRIVS) {
	    pl->nonewprivs = int_value;
	} else {
	    if (pl->login_limit_def < source) {
		return;
	    } else {
		pl->login_limit = int_value;
		pl->login_limit_def = source;
	    }
	}
    }
}

static int
parse_uid_range(pam_handle_t *pamh, const char *domain,
		uid_t *min_uid, uid_t *max_uid)
{
    const char *range = domain;
    char *pmax;
    char *endptr;
    int rv = LIMIT_RANGE_MM;

    if ((pmax=strchr(range, ':')) == NULL)
	return LIMIT_RANGE_NONE;
    ++pmax;

    if (range[0] == '@' || range[0] == '%')
	++range;

    if (range[0] == ':')
	rv = LIMIT_RANGE_ONE;
    else {
	    errno = 0;
	    *min_uid = strtoul (range, &endptr, 10);
	    if (errno != 0 || (range == endptr) || *endptr != ':') {
		pam_syslog(pamh, LOG_DEBUG,
			   "wrong min_uid/gid value in '%s'", domain);
		return LIMIT_RANGE_ERR;
	    }
    }

    if (*pmax == '\0') {
	if (rv == LIMIT_RANGE_ONE)
	    return LIMIT_RANGE_ERR;
	else
	    return LIMIT_RANGE_MIN;
    }

    errno = 0;
    *max_uid = strtoul (pmax, &endptr, 10);
    if (errno != 0 || (pmax == endptr) || *endptr != '\0') {
	pam_syslog(pamh, LOG_DEBUG,
		   "wrong max_uid/gid value in '%s'", domain);
	return LIMIT_RANGE_ERR;
    }

    if (rv == LIMIT_RANGE_ONE)
	*min_uid = *max_uid;
    return rv;
}

static int
set_if_null(char **dest, char *def)
{
    if (*dest == NULL) {
	*dest = def;
	return 0;
    }
    return 1;
}

static char *
trim(char *s)
{
    char *p;

    if (s == NULL)
	return NULL;

    while (*s == ' ' || *s == '\t')
	s++;

    if (*s == '\0')
	return NULL;

    p = s + strlen(s) - 1;
    while (p >= s && (*p == ' ' || *p == '\t'))
	*p-- = '\0';
    return s;
}

static int
split(char *line, char **domain, char **ltype, char **item, char **value)
{
    char *blank, *saveptr;
    int count;

    blank = line + strlen(line);
    saveptr = NULL;

    *domain = strtok_r(line, " \t", &saveptr);
    *ltype = strtok_r(NULL, " \t", &saveptr);
    *item = strtok_r(NULL, " \t", &saveptr);
    *value = trim(strtok_r(NULL, "", &saveptr));

    count = 0;
    count += set_if_null(domain, blank);
    count += set_if_null(ltype, blank);
    count += set_if_null(item, blank);
    count += set_if_null(value, blank);

    return count;
}

static int
parse_config_file(pam_handle_t *pamh, const char *uname, uid_t uid, gid_t gid,
		  int ctrl, struct pam_limit_s *pl, const int conf_file_set_by_user)
{
    FILE *fil;
    char *buf = NULL;
    size_t n = 0;
    unsigned long long lineno = 0;

    /* check for the conf_file */
    if (ctrl & PAM_DEBUG_ARG)
        pam_syslog(pamh, LOG_DEBUG, "reading settings from '%s'", pl->conf_file);
    fil = fopen(pl->conf_file, "r");
    if (fil == NULL) {
        if (errno == ENOENT && !conf_file_set_by_user)
            return PAM_SUCCESS; /* file is not there and it has not been set by the conf= argument */

        pam_syslog(pamh, LOG_WARNING,
                   "cannot read settings from %s: %s", pl->conf_file,
                   strerror(errno));
        return PAM_SERVICE_ERR;
    }

    /* start the show */
    while (getline(&buf, &n, fil) != -1) {
        char *domain, *ltype, *item, *value, *tptr, *line;
        int i;
        int rngtype;
        size_t j;
        uid_t min_uid = (uid_t)-1, max_uid = (uid_t)-1;

        lineno++;

        line = buf;
        /* skip the leading white space */
        while (*line && isspace((unsigned char)*line))
            line++;

        /* Rip off the comments */
        tptr = strchr(line,'#');
        if (tptr)
            *tptr = '\0';
        /* Rip off the newline char */
        tptr = strchr(line,'\n');
        if (tptr)
            *tptr = '\0';
        /* Anything left ? */
        if (!strlen(line))
            continue;

	i = split(line, &domain, &ltype, &item, &value);
	D(("scanned line[%d]: domain[%s], ltype[%s], item[%s], value[%s]",
	   i, domain, ltype, item, value));

        for(j=0; j < strlen(ltype); j++)
            ltype[j]=tolower((unsigned char)ltype[j]);

	if ((rngtype=parse_uid_range(pamh, domain, &min_uid, &max_uid)) < 0) {
	    pam_syslog(pamh, LOG_WARNING, "invalid uid range '%s' - skipped", domain);
	    continue;
	}

        if (i == 4) { /* a complete line */
	    for(j=0; j < strlen(item); j++)
		item[j]=tolower((unsigned char)item[j]);
	    for(j=0; j < strlen(value); j++)
		value[j]=tolower((unsigned char)value[j]);

            if (strcmp(uname, domain) == 0) /* this user has a limit */
                process_limit(pamh, LIMITS_DEF_USER, ltype, item, value, ctrl, pl);
            else if (domain[0]=='@') {
		if (ctrl & PAM_DEBUG_ARG) {
			pam_syslog(pamh, LOG_DEBUG,
				   "checking if %s is in group %s",
				   uname, domain + 1);
		}
		switch(rngtype) {
		    case LIMIT_RANGE_NONE:
			if (pam_modutil_user_in_group_nam_nam(pamh, uname, domain+1))
			    process_limit(pamh, LIMITS_DEF_GROUP, ltype, item, value, ctrl,
					  pl);
			break;
		    case LIMIT_RANGE_ONE:
			if (pam_modutil_user_in_group_nam_gid(pamh, uname, (gid_t)max_uid))
			    process_limit(pamh, LIMITS_DEF_GROUP, ltype, item, value, ctrl,
				  pl);
			break;
		    case LIMIT_RANGE_MM:
			if (gid > (gid_t)max_uid)
			    break;
			/* fallthrough */
		    case LIMIT_RANGE_MIN:
			if (gid >= (gid_t)min_uid)
			    process_limit(pamh, LIMITS_DEF_GROUP, ltype, item, value, ctrl,
					  pl);
		}
            } else if (domain[0]=='%') {
		if (ctrl & PAM_DEBUG_ARG) {
			pam_syslog(pamh, LOG_DEBUG,
				   "checking if %s is in group %s",
				   uname, domain + 1);
		}
		switch(rngtype) {
		    case LIMIT_RANGE_NONE:
			if (strcmp(domain,"%") == 0)
			    process_limit(pamh, LIMITS_DEF_ALL, ltype, item, value, ctrl,
					  pl);
			else if (pam_modutil_user_in_group_nam_nam(pamh, uname, domain+1)) {
			    free(pl->login_group);
			    pl->login_group = strdup(domain+1);
			    process_limit(pamh, LIMITS_DEF_ALLGROUP, ltype, item, value, ctrl,
					  pl);
			}
			break;
		    case LIMIT_RANGE_ONE:
			if (pam_modutil_user_in_group_nam_gid(pamh, uname, (gid_t)max_uid)) {
			    struct group *grp;
			    grp = pam_modutil_getgrgid(pamh, (gid_t)max_uid);
			    free(pl->login_group);
			    pl->login_group = strdup(grp->gr_name);
			    process_limit(pamh, LIMITS_DEF_ALLGROUP, ltype, item, value, ctrl,
					  pl);
			}
			break;
		    case LIMIT_RANGE_MIN:
		    case LIMIT_RANGE_MM:
			pam_syslog(pamh, LOG_WARNING, "range unsupported for %%group matching - ignored");
		}
            } else {
		switch(rngtype) {
		    case LIMIT_RANGE_NONE:
			if (strcmp(domain, "*") == 0)
			    process_limit(pamh, LIMITS_DEF_DEFAULT, ltype, item, value, ctrl,
					  pl);
			break;
		    case LIMIT_RANGE_ONE:
			if (uid != max_uid)
			    break;
			/* fallthrough */
		    case LIMIT_RANGE_MM:
			if (uid > max_uid)
			    break;
			/* fallthrough */
		    case LIMIT_RANGE_MIN:
			if (uid >= min_uid)
			    process_limit(pamh, LIMITS_DEF_USER, ltype, item, value, ctrl, pl);
		}
	    }
	} else if (i == 2 && ltype[0] == '-') { /* Probably a no-limit line */
	    if (strcmp(uname, domain) == 0) {
		if (ctrl & PAM_DEBUG_ARG) {
		    pam_syslog(pamh, LOG_DEBUG, "no limits for '%s'", uname);
		}
	    } else if (domain[0] == '@') {
		switch(rngtype) {
		    case LIMIT_RANGE_NONE:
			if (!pam_modutil_user_in_group_nam_nam(pamh, uname, domain+1))
			    continue; /* next line */
			break;
		    case LIMIT_RANGE_ONE:
			if (!pam_modutil_user_in_group_nam_gid(pamh, uname, (gid_t)max_uid))
			    continue; /* next line */
			break;
		    case LIMIT_RANGE_MM:
			if (gid > (gid_t)max_uid)
			    continue;  /* next line */
			/* fallthrough */
		    case LIMIT_RANGE_MIN:
			if (gid < (gid_t)min_uid)
			    continue;  /* next line */
		}
		if (ctrl & PAM_DEBUG_ARG) {
		    pam_syslog(pamh, LOG_DEBUG,
			       "no limits for '%s' in group '%s'",
			       uname, domain+1);
		}
	    } else {
		switch(rngtype) {
		    case LIMIT_RANGE_NONE:
			continue;  /* next line */
		    case LIMIT_RANGE_ONE:
			if (uid != max_uid)
			    continue;  /* next line */
			break;
		    case LIMIT_RANGE_MM:
			if (uid > max_uid)
			    continue;  /* next line */
			/* fallthrough */
		    case LIMIT_RANGE_MIN:
			if (uid >= min_uid)
			    break;
			continue;  /* next line */
		}
		if (ctrl & PAM_DEBUG_ARG) {
		    pam_syslog(pamh, LOG_DEBUG, "no limits for '%s'", uname);
		}
	    }
	    free(buf);
	    fclose(fil);
	    return PAM_IGNORE;
        } else {
            pam_syslog(pamh, LOG_WARNING, "invalid line %llu in '%s' - skipped",
		       lineno, pl->conf_file);
	}
    }
    free(buf);
    fclose(fil);
    return PAM_SUCCESS;
}

static int setup_limits(pam_handle_t *pamh,
			const char *uname, uid_t uid, int ctrl,
			struct pam_limit_s *pl)
{
    int i;
    int status;
    int retval = LIMITED_OK;

    for (i=0, status=LIMITED_OK; i<RLIM_NLIMITS; i++) {
      int res;

	if (!pl->limits[i].supported) {
	    /* skip it if its not known to the system */
	    continue;
	}
	if (pl->limits[i].src_soft == LIMITS_DEF_NONE &&
	    pl->limits[i].src_hard == LIMITS_DEF_NONE) {
	    /* skip it if its not initialized */
	    continue;
	}
        if (pl->limits[i].limit.rlim_cur > pl->limits[i].limit.rlim_max)
            pl->limits[i].limit.rlim_cur = pl->limits[i].limit.rlim_max;
	res = setrlimit(i, &pl->limits[i].limit);
	if (res != 0)
	  pam_syslog(pamh, LOG_ERR, "Could not set limit for '%s': %m",
		     rlimit2str(i));
	status |= res;
    }

    if (status) {
        retval = LIMIT_ERR;
    }

    status = setpriority(PRIO_PROCESS, 0, pl->priority);
    if (status != 0) {
        pam_syslog(pamh, LOG_ERR, "Could not set limit for PRIO_PROCESS: %m");
        retval = LIMIT_ERR;
    }

    if (uid == 0) {
	D(("skip login limit check for uid=0"));
    } else if (pl->login_limit > 0) {
        if (check_logins(pamh, uname, pl->login_limit, ctrl, pl) == LOGIN_ERR) {
#ifdef HAVE_LIBAUDIT
	    if (!(ctrl & PAM_NO_AUDIT)) {
		pam_modutil_audit_write(pamh, AUDIT_ANOM_LOGIN_SESSIONS,
		    "pam_limits", PAM_PERM_DENIED);
		/* ignore return value as we fail anyway */
            }
#endif
            retval |= LOGIN_ERR;
	}
    } else if (pl->login_limit == 0) {
        retval |= LOGIN_ERR;
    }

    if (pl->nonewprivs) {
#ifdef __linux__
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
	    pam_syslog(pamh, LOG_ERR, "Could not set prctl(PR_SET_NO_NEW_PRIVS): %m");
	    retval |= LIMIT_ERR;
	}
#else
	pam_syslog(pamh, LOG_INFO, "Setting 'nonewprivs' not supported on this OS");
#endif
    }

    return retval;
}

/* --- evaluating all files in VENDORDIR/security/limits.d and /etc/security/limits.d --- */
static const char *
base_name(const char *path)
{
    const char *base = strrchr(path, '/');
    return base ? base+1 : path;
}

static int
compare_filename(const void *a, const void *b)
{
	return strcmp(base_name(* (const char * const *) a),
		      base_name(* (const char * const *) b));
}

/* Evaluating a list of files which have to be parsed in the right order:
 *
 * - If etc/security/limits.d/@filename@.conf exists, then
 *   %vendordir%/security/limits.d/@filename@.conf should not be used.
 * - All files in both limits.d directories are sorted by their @filename@.conf in
 *   lexicographic order regardless of which of the directories they reside in. */
static char **
read_limits_dir(pam_handle_t *pamh)
{
	glob_t globbuf;
	size_t i=0;
	int glob_rv = glob(LIMITS_CONF_GLOB, GLOB_ERR | GLOB_NOSORT, NULL, &globbuf);
	char **file_list;
	size_t file_list_size = glob_rv == 0 ? globbuf.gl_pathc : 0;

#ifdef VENDOR_LIMITS_CONF_GLOB
	glob_t globbuf_vendor;
	int glob_rv_vendor = glob(VENDOR_LIMITS_CONF_GLOB, GLOB_ERR | GLOB_NOSORT, NULL, &globbuf_vendor);
	if (glob_rv_vendor == 0)
	    file_list_size += globbuf_vendor.gl_pathc;
#endif
	file_list = malloc((file_list_size + 1) * sizeof(char*));
	if (file_list == NULL) {
	    pam_syslog(pamh, LOG_ERR, "Cannot allocate memory for file list: %m");
#ifdef VENDOR_ACCESS_CONF_GLOB
            if (glob_rv_vendor == 0)
                globfree(&globbuf_vendor);
#endif
            if (glob_rv == 0)
                globfree(&globbuf);
	    return NULL;
	}

	if (glob_rv == 0) {
	    for (i = 0; i < globbuf.gl_pathc; i++) {
	        file_list[i] = strdup(globbuf.gl_pathv[i]);
		if (file_list[i] == NULL) {
		    pam_syslog(pamh, LOG_ERR, "strdup failed: %m");
		    break;
		}
	    }
	}
#ifdef VENDOR_LIMITS_CONF_GLOB
	if (glob_rv_vendor == 0) {
	    for (size_t j = 0; j < globbuf_vendor.gl_pathc; j++) {
		if (glob_rv == 0 && globbuf.gl_pathc > 0) {
		    int double_found = 0;
		    for (size_t k = 0; k < globbuf.gl_pathc; k++) {
			if (strcmp(base_name(globbuf.gl_pathv[k]),
				   base_name(globbuf_vendor.gl_pathv[j])) == 0) {
				double_found = 1;
				break;
			}
		    }
		    if (double_found)
			continue;
		}
		file_list[i] = strdup(globbuf_vendor.gl_pathv[j]);
		if (file_list[i] == NULL) {
		    pam_syslog(pamh, LOG_ERR, "strdup failed: %m");
		    break;
		}
		i++;
	    }
	    globfree(&globbuf_vendor);
	}
#endif
	file_list[i] = NULL;
	qsort(file_list, i, sizeof(char *), compare_filename);
        if (glob_rv == 0)
	    globfree(&globbuf);

	return file_list;
}

/* now the session stuff */
int
pam_sm_open_session (pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
    int retval, i;
    char *user_name;
    struct passwd *pwd;
    int ctrl;
    struct pam_limit_s plstruct;
    struct pam_limit_s *pl = &plstruct;
    char *free_filename = NULL;

    D(("called."));

    memset(pl, 0, sizeof(*pl));

    ctrl = _pam_parse(pamh, argc, argv, pl);
    retval = pam_get_item( pamh, PAM_USER, (void*) &user_name );
    if ( user_name == NULL || retval != PAM_SUCCESS ) {
        pam_syslog(pamh, LOG_ERR, "open_session - error recovering username");
        return PAM_SESSION_ERR;
    }

    int conf_file_set_by_user = (pl->conf_file != NULL);
    if (pl->conf_file == NULL) {
        pl->conf_file = LIMITS_FILE;
#ifdef VENDOR_LIMITS_FILE
        /*
         * Check whether LIMITS_FILE file is available.
         * If it does not exist, fall back to VENDOR_LIMITS_FILE file.
         */
        struct stat buffer;
        if (stat(pl->conf_file, &buffer) != 0 && errno == ENOENT)
            pl->conf_file = VENDOR_LIMITS_FILE;
#endif
    }

    pwd = pam_modutil_getpwnam(pamh, user_name);
    if (!pwd) {
        if (ctrl & PAM_DEBUG_ARG)
            pam_syslog(pamh, LOG_WARNING,
		       "open_session username '%s' does not exist", user_name);
        return PAM_USER_UNKNOWN;
    }

    retval = init_limits(pamh, pl, ctrl);
    if (retval != PAM_SUCCESS) {
        pam_syslog(pamh, LOG_ERR, "cannot initialize");
        return PAM_ABORT;
    }

    retval = parse_config_file(pamh, pwd->pw_name, pwd->pw_uid, pwd->pw_gid,
			       ctrl, pl, conf_file_set_by_user);
    if (retval == PAM_IGNORE) {
	D(("the configuration file ('%s') has an applicable '<domain> -' entry", pl->conf_file));
	free(pl->login_group);
	return PAM_SUCCESS;
    }
    if (retval != PAM_SUCCESS || conf_file_set_by_user)
	/* skip reading limits.d if config file explicitly specified */
	goto out;

    /* Read subsequent *.conf files, if they exist. */
    char **filename_list = read_limits_dir(pamh);
    if (filename_list != NULL) {
        for (i = 0; filename_list[i] != NULL; i++) {
            pl->conf_file = filename_list[i];
            retval = parse_config_file(pamh, pwd->pw_name, pwd->pw_uid, pwd->pw_gid, ctrl, pl, 0);
            if (retval != PAM_SUCCESS)
                break;
        }
        for (i = 0; filename_list[i] != NULL; i++) {
	    if (filename_list[i] == pl->conf_file)
		free_filename = filename_list[i];
	    else
		free(filename_list[i]);
	}
        free(filename_list);
    }

    if (retval == PAM_IGNORE) {
        D(("the configuration file ('%s') has an applicable '<domain> -' entry", pl->conf_file));
        free(free_filename);
        free(pl->login_group);
        return PAM_SUCCESS;
    }

out:
    if (retval != PAM_SUCCESS)
    {
	pam_syslog(pamh, LOG_ERR, "error parsing the configuration file: '%s' ", pl->conf_file);
	free(free_filename);
	free(pl->login_group);
	return retval;
    }

    retval = setup_limits(pamh, pwd->pw_name, pwd->pw_uid, ctrl, pl);
    free(free_filename);
    free(pl->login_group);
    if (retval & LOGIN_ERR)
	pam_error(pamh, _("There were too many logins for '%s'."),
		  pwd->pw_name);
    if (retval != LIMITED_OK) {
        return PAM_PERM_DENIED;
    }

    return PAM_SUCCESS;
}

int
pam_sm_close_session (pam_handle_t *pamh UNUSED, int flags UNUSED,
		      int argc UNUSED, const char **argv UNUSED)
{
     /* nothing to do */
     return PAM_SUCCESS;
}

/*
 * Copyright (c) Cristian Gafton, 1996-1997, <gafton@redhat.com>
 *                                              All rights reserved.
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
 */
