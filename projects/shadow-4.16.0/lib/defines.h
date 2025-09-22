/* $Id$ */
/* some useful defines */

#ifndef _DEFINES_H_
#define _DEFINES_H_

#include "config.h"

#include <stdbool.h>
#include <locale.h>

#define gettext_noop(String) (String)
/* #define gettext_def(String) "#define String" */

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# undef bindtextdomain
# define bindtextdomain(Domain, Directory)	(NULL)
# undef textdomain
# define textdomain(Domain)	(NULL)
# define _(Text) Text
# define ngettext(Msgid1, Msgid2, N) \
    ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>

/*
 * crypt(3), crypt_gensalt(3), and their
 * feature test macros may be defined in here.
 */
#if HAVE_CRYPT_H
# include <crypt.h>
#endif

#include <sys/time.h>
#include <time.h>

#include <dirent.h>

/*
 * Possible cases:
 * - /usr/include/shadow.h exists and includes the shadow group stuff.
 * - /usr/include/shadow.h exists, but we use our own gshadow.h.
 */
#include <shadow.h>
#if defined(SHADOWGRP) && !defined(GSHADOW)
#include "gshadow_.h"
#endif

#include <limits.h>

#ifndef	NGROUPS_MAX
#ifdef	NGROUPS
#define	NGROUPS_MAX	NGROUPS
#else
#define	NGROUPS_MAX	64
#endif
#endif

#include <syslog.h>

#ifndef LOG_WARN
#define LOG_WARN LOG_WARNING
#endif

/* LOG_NOWAIT is deprecated */
#ifndef LOG_NOWAIT
#define LOG_NOWAIT 0
#endif

/* LOG_AUTH is deprecated, use LOG_AUTHPRIV instead */
#ifndef LOG_AUTHPRIV
#define LOG_AUTHPRIV LOG_AUTH
#endif

/* cleaner than lots of #ifdefs everywhere - use this as follows:
   SYSLOG((LOG_CRIT, "user %s cracked root", user)); */
#ifdef ENABLE_NLS
/* Temporarily set LC_TIME to "C" to avoid strange dates in syslog.
   This is a workaround for a more general syslog(d) design problem -
   syslogd should log the current system time for each event, and not
   trust the formatted time received from the unix domain (or worse,
   UDP) socket.  -MM */
/* Avoid translated PAM error messages: Set LC_ALL to "C".
 * --Nekral */
#define SYSLOG(x)							\
	do {								\
		char *old_locale = setlocale (LC_ALL, NULL);		\
		char *saved_locale = NULL;				\
		if (NULL != old_locale) {				\
			saved_locale = strdup (old_locale);		\
		}							\
		if (NULL != saved_locale) {				\
			(void) setlocale (LC_ALL, "C");			\
		}							\
		syslog x ;						\
		if (NULL != saved_locale) {				\
			(void) setlocale (LC_ALL, saved_locale);	\
			free (saved_locale);				\
		}							\
	} while (false)
#else				/* !ENABLE_NLS */
#define SYSLOG(x) syslog x
#endif				/* !ENABLE_NLS */

/* The default syslog settings can now be changed here,
   in just one place.  */

#ifndef SYSLOG_OPTIONS
/* #define SYSLOG_OPTIONS (LOG_PID | LOG_CONS | LOG_NOWAIT) */
#define SYSLOG_OPTIONS (LOG_PID)
#endif

#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_AUTHPRIV
#endif

#define OPENLOG(progname) openlog(progname, SYSLOG_OPTIONS, SYSLOG_FACILITY)

#include <termios.h>
#define STTY(fd, termio) tcsetattr(fd, TCSANOW, termio)
#define GTTY(fd, termio) tcgetattr(fd, termio)
#define TERMIO struct termios

/*
 * Password aging constants
 *
 * DAY - seconds / day
 * WEEK - seconds / week
 */

/* Solaris defines this in shadow.h */
#ifndef DAY
#define DAY  ((time_t) 24 * 3600)
#endif

#define WEEK (7*DAY)

#ifndef PASSWD_FILE
#define PASSWD_FILE "/etc/passwd"
#endif

#ifndef GROUP_FILE
#define GROUP_FILE "/etc/group"
#endif

#ifndef SHADOW_FILE
#define SHADOW_FILE "/etc/shadow"
#endif

#ifndef SUBUID_FILE
#define SUBUID_FILE "/etc/subuid"
#endif

#ifndef SUBGID_FILE
#define SUBGID_FILE "/etc/subgid"
#endif

#ifdef SHADOWGRP
#ifndef SGROUP_FILE
#define SGROUP_FILE "/etc/gshadow"
#endif
#endif

/*
 * string to use for the pw_passwd field in /etc/passwd when using
 * shadow passwords - most systems use "x" but there are a few
 * exceptions, so it can be changed here if necessary.  --marekm
 */
#ifndef SHADOW_PASSWD_STRING
#define SHADOW_PASSWD_STRING "x"
#endif

#define SHADOW_SP_FLAG_UNSET ((unsigned long)-1)

#ifdef WITH_AUDIT
/* in case we use pam < 0.80 */
#undef __u8
#undef __u32

#include <libaudit.h>
#endif

/* Maximum length of passwd entry */
#define PASSWD_ENTRY_MAX_LENGTH 32768

#ifdef HAVE_SECURE_GETENV
#  define shadow_getenv(name) secure_getenv(name)
# else
#  define shadow_getenv(name) getenv(name)
#endif

/*
 * Maximum password length
 *
 * Consider that there is also limit in PAM (PAM_MAX_RESP_SIZE)
 * currently set to 512.
 */
#if !defined(PASS_MAX)
#define PASS_MAX  BUFSIZ - 1
#endif

#endif				/* _DEFINES_H_ */
