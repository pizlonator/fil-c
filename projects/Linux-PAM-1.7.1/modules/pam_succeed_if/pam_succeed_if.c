/******************************************************************************
 * A simple user-attribute based module for PAM.
 *
 * Copyright (c) 2003 Red Hat, Inc.
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
 */

#include "config.h"

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <netdb.h>

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_inline.h"

/* Basically, run cmp(atol(left), atol(right)), returning PAM_SUCCESS if
 * the function returns non-zero, PAM_AUTH_ERR if it returns zero, and
 * PAM_SERVICE_ERR if the arguments can't be parsed as numbers. */
static int
evaluate_num(const pam_handle_t *pamh, const char *left,
	     const char *right, int (*cmp)(long long, long long))
{
	long long l, r;
	char *p;
	int ret = PAM_SUCCESS;

	errno = 0;
	l = strtoll(left, &p, 0);
	if ((p == NULL) || (*p != '\0') || (p == left) || errno) {
		pam_syslog(pamh, LOG_INFO, "\"%s\" is not a number", left);
		ret = PAM_SERVICE_ERR;
	}

	r = strtoll(right, &p, 0);
	if ((p == NULL) || (*p != '\0') || (p == right) || errno) {
		pam_syslog(pamh, LOG_INFO, "\"%s\" is not a number", right);
		ret = PAM_SERVICE_ERR;
	}

	if (ret != PAM_SUCCESS) {
		return ret;
	}

	return cmp(l, r) ? PAM_SUCCESS : PAM_AUTH_ERR;
}

/* Simple numeric comparison callbacks. */
static int
eq(long long i, long long j)
{
	return i == j;
}
static int
ne(long long i, long long j)
{
	return i != j;
}
static int
lt(long long i, long long j)
{
	return i < j;
}
static int
le(long long i, long long j)
{
	return lt(i, j) || eq(i, j);
}
static int
gt(long long i, long long j)
{
	return i > j;
}
static int
ge(long long i, long long j)
{
	return gt(i, j) || eq(i, j);
}

/* Test for numeric equality. */
static int
evaluate_eqn(const pam_handle_t *pamh, const char *left, const char *right)
{
	return evaluate_num(pamh, left, right, eq);
}
/* Test for string equality. */
static int
evaluate_eqs(const char *left, const char *right)
{
	return (strcmp(left, right) == 0) ? PAM_SUCCESS : PAM_AUTH_ERR;
}
/* Test for numeric inequality. */
static int
evaluate_nen(const pam_handle_t *pamh, const char *left, const char *right)
{
	return evaluate_num(pamh, left, right, ne);
}
/* Test for string inequality. */
static int
evaluate_nes(const char *left, const char *right)
{
	return (strcmp(left, right) != 0) ? PAM_SUCCESS : PAM_AUTH_ERR;
}
/* Test for numeric less-than-ness(?) */
static int
evaluate_lt(const pam_handle_t *pamh, const char *left, const char *right)
{
	return evaluate_num(pamh, left, right, lt);
}
/* Test for numeric less-than-or-equal-ness(?) */
static int
evaluate_le(const pam_handle_t *pamh, const char *left, const char *right)
{
	return evaluate_num(pamh, left, right, le);
}
/* Test for numeric greater-than-ness(?) */
static int
evaluate_gt(const pam_handle_t *pamh, const char *left, const char *right)
{
	return evaluate_num(pamh, left, right, gt);
}
/* Test for numeric greater-than-or-equal-ness(?) */
static int
evaluate_ge(const pam_handle_t *pamh, const char *left, const char *right)
{
	return evaluate_num(pamh, left, right, ge);
}
/* Check for file glob match. */
static int
evaluate_glob(const char *left, const char *right)
{
	return (fnmatch(right, left, 0) == 0) ? PAM_SUCCESS : PAM_AUTH_ERR;
}
/* Check for file glob mismatch. */
static int
evaluate_noglob(const char *left, const char *right)
{
	return (fnmatch(right, left, 0) != 0) ? PAM_SUCCESS : PAM_AUTH_ERR;
}
/* Check for list match. */
static int
evaluate_inlist(const char *left, const char *right)
{
	char *p;
	/* Don't care about left containing ':'. */
	while ((p=strstr(right, left)) != NULL) {
		if (p == right || *(p-1) == ':') { /* ':' is a list separator */
			p += strlen(left);
			if (*p == '\0' || *p == ':') {
				return PAM_SUCCESS;
			}
		}
		right = strchr(p, ':');
		if (right == NULL)
			break;
		else
			++right;
	}
	return PAM_AUTH_ERR;
}
/* Check for list mismatch. */
static int
evaluate_notinlist(const char *left, const char *right)
{
	return evaluate_inlist(left, right) != PAM_SUCCESS ? PAM_SUCCESS : PAM_AUTH_ERR;
}
/* Return PAM_SUCCESS if the user is in the group. */
static int
evaluate_ingroup(pam_handle_t *pamh, const char *user, const char *grouplist)
{
	char *ptr = NULL;
	static const char delim[] = ":";
	char const *grp = NULL;
	char *group = strdup(grouplist);

	if (group == NULL)
		return PAM_BUF_ERR;

	grp = strtok_r(group, delim, &ptr);
	while(grp != NULL) {
		if (pam_modutil_user_in_group_nam_nam(pamh, user, grp) == 1) {
			free(group);
			return PAM_SUCCESS;
		}
		grp = strtok_r(NULL, delim, &ptr);
	}
	free(group);
	return PAM_AUTH_ERR;
}
/* Return PAM_SUCCESS if the user is NOT in the group. */
static int
evaluate_notingroup(pam_handle_t *pamh, const char *user, const char *grouplist)
{
	char *ptr = NULL;
	static const char delim[] = ":";
	char const *grp = NULL;
	char *group = strdup(grouplist);

	if (group == NULL)
		return PAM_BUF_ERR;

	grp = strtok_r(group, delim, &ptr);
	while(grp != NULL) {
		if (pam_modutil_user_in_group_nam_nam(pamh, user, grp) == 1) {
			free(group);
			return PAM_AUTH_ERR;
		}
		grp = strtok_r(NULL, delim, &ptr);
	}
	free(group);
	return PAM_SUCCESS;
}

#ifdef HAVE_INNETGR
# define SOMETIMES_UNUSED UNUSED
#else
# define SOMETIMES_UNUSED
#endif

/* Return PAM_SUCCESS if the (host,user) is in the netgroup. */
static int
evaluate_innetgr(const pam_handle_t* pamh SOMETIMES_UNUSED, const char *host, const char *user, const char *group)
{
#ifdef HAVE_INNETGR
	if (innetgr(group, host, user, NULL) == 1)
		return PAM_SUCCESS;
#else
	pam_syslog (pamh, LOG_ERR, "pam_succeed_if does not have netgroup support");
#endif

	return PAM_AUTH_ERR;
}
/* Return PAM_SUCCESS if the (host,user) is NOT in the netgroup. */
static int
evaluate_notinnetgr(const pam_handle_t* pamh SOMETIMES_UNUSED, const char *host, const char *user, const char *group)
{
#ifdef HAVE_INNETGR
	if (innetgr(group, host, user, NULL) == 0)
		return PAM_SUCCESS;
#else
	pam_syslog (pamh, LOG_ERR, "pam_succeed_if does not have netgroup support");
#endif
	return PAM_AUTH_ERR;
}

/* Match a triple. */
static int
evaluate(pam_handle_t *pamh, int debug,
	 const char *left, const char *qual, const char *right,
	 struct passwd **pwd, const char *user)
{
	char numstr[sizeof(long) * 3 + 1] = "";
	const char *attribute = left;
	/* Get information about the user if needed. */
	if ((*pwd == NULL) &&
	    ((strcasecmp(left, "uid") == 0) ||
	     (strcasecmp(left, "gid") == 0) ||
	     (strcasecmp(left, "shell") == 0) ||
	     (strcasecmp(left, "home") == 0) ||
	     (strcasecmp(left, "dir") == 0) ||
	     (strcasecmp(left, "homedir") == 0))) {
		*pwd = pam_modutil_getpwnam(pamh, user);
		if (*pwd == NULL) {
			return PAM_USER_UNKNOWN;
		}
	}
	/* Figure out what we're evaluating here, and convert it to a string.*/
	if ((strcasecmp(left, "login") == 0) ||
	    (strcasecmp(left, "name") == 0) ||
	    (strcasecmp(left, "user") == 0)) {
		left = user;
	} else if (strcasecmp(left, "uid") == 0) {
		pam_sprintf(numstr, "%lu", (unsigned long) (*pwd)->pw_uid);
		left = numstr;
	} else if (strcasecmp(left, "gid") == 0) {
		pam_sprintf(numstr, "%lu", (unsigned long) (*pwd)->pw_gid);
		left = numstr;
	} else if (strcasecmp(left, "shell") == 0) {
		left = (*pwd)->pw_shell;
	} else if ((strcasecmp(left, "home") == 0) ||
	    (strcasecmp(left, "dir") == 0) ||
	    (strcasecmp(left, "homedir") == 0)) {
		left = (*pwd)->pw_dir;
	} else if (strcasecmp(left, "service") == 0) {
		const void *svc;
		if (pam_get_item(pamh, PAM_SERVICE, &svc) != PAM_SUCCESS ||
			svc == NULL)
			svc = "";
		left = (const char *)svc;
	} else if (strcasecmp(left, "ruser") == 0) {
		const void *ruser;
		if (pam_get_item(pamh, PAM_RUSER, &ruser) != PAM_SUCCESS ||
			ruser == NULL)
			ruser = "";
		left = (const char *)ruser;
	} else if (strcasecmp(left, "rhost") == 0) {
		const void *rhost;
		if (pam_get_item(pamh, PAM_RHOST, &rhost) != PAM_SUCCESS ||
			rhost == NULL)
			rhost = "";
		left = (const char *)rhost;
	} else if (strcasecmp(left, "tty") == 0) {
		const void *tty;
		if (pam_get_item(pamh, PAM_TTY, &tty) != PAM_SUCCESS ||
			tty == NULL)
			tty = "";
		left = (const char *)tty;
	} else {
		/* If we have no idea what's going on, return an error. */
		pam_syslog(pamh, LOG_ERR, "unknown attribute \"%s\"", left);
		return PAM_SERVICE_ERR;
	}
	if (debug) {
		pam_syslog(pamh, LOG_DEBUG, "'%s' resolves to '%s'",
			   attribute, left);
	}

	/* Attribute value < some threshold. */
	if ((strcasecmp(qual, "<") == 0) ||
	    (strcasecmp(qual, "lt") == 0)) {
		return evaluate_lt(pamh, left, right);
	}
	/* Attribute value <= some threshold. */
	if ((strcasecmp(qual, "<=") == 0) ||
	    (strcasecmp(qual, "le") == 0)) {
		return evaluate_le(pamh, left, right);
	}
	/* Attribute value > some threshold. */
	if ((strcasecmp(qual, ">") == 0) ||
	    (strcasecmp(qual, "gt") == 0)) {
		return evaluate_gt(pamh, left, right);
	}
	/* Attribute value >= some threshold. */
	if ((strcasecmp(qual, ">=") == 0) ||
	    (strcasecmp(qual, "ge") == 0)) {
		return evaluate_ge(pamh, left, right);
	}
	/* Attribute value == some threshold. */
	if (strcasecmp(qual, "eq") == 0) {
		return evaluate_eqn(pamh, left, right);
	}
	/* Attribute value = some string. */
	if (strcasecmp(qual, "=") == 0) {
		return evaluate_eqs(left, right);
	}
	/* Attribute value != some threshold. */
	if (strcasecmp(qual, "ne") == 0) {
		return evaluate_nen(pamh, left, right);
	}
	/* Attribute value != some string. */
	if (strcasecmp(qual, "!=") == 0) {
		return evaluate_nes(left, right);
	}
	/* Attribute value matches some pattern. */
	if ((strcasecmp(qual, "=~") == 0) ||
	    (strcasecmp(qual, "glob") == 0)) {
		return evaluate_glob(left, right);
	}
	if ((strcasecmp(qual, "!~") == 0) ||
	    (strcasecmp(qual, "noglob") == 0)) {
		return evaluate_noglob(left, right);
	}
	/* Attribute value matches item in list. */
	if (strcasecmp(qual, "in") == 0) {
		return evaluate_inlist(left, right);
	}
	if (strcasecmp(qual, "notin") == 0) {
		return evaluate_notinlist(left, right);
	}
	/* User is in this group(s). */
	if (strcasecmp(qual, "ingroup") == 0) {
		return evaluate_ingroup(pamh, user, right);
	}
	/* User is not in this group(s). */
	if (strcasecmp(qual, "notingroup") == 0) {
		return evaluate_notingroup(pamh, user, right);
	}
	/* (Rhost, user) is in this netgroup. */
	if (strcasecmp(qual, "innetgr") == 0) {
		const void *rhost;
		if (pam_get_item(pamh, PAM_RHOST, &rhost) != PAM_SUCCESS)
			rhost = NULL;
		return evaluate_innetgr(pamh, rhost, user, right);
	}
	/* (Rhost, user) is not in this group. */
	if (strcasecmp(qual, "notinnetgr") == 0) {
		const void *rhost;
		if (pam_get_item(pamh, PAM_RHOST, &rhost) != PAM_SUCCESS)
			rhost = NULL;
		return evaluate_notinnetgr(pamh, rhost, user, right);
	}
	/* Fail closed. */
	return PAM_SERVICE_ERR;
}

static int
pam_succeed_if(pam_handle_t *pamh, int argc, const char **argv)
{
	const char *user;
	struct passwd *pwd = NULL;
	int ret, i, count, use_uid, debug;
	const char *left, *right, *qual;
	int quiet_fail, quiet_succ, audit;

	quiet_fail = 0;
	quiet_succ = 0;
	audit = 0;
	for (use_uid = 0, debug = 0, i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0) {
			debug++;
		}
		if (strcmp(argv[i], "use_uid") == 0) {
			use_uid++;
		}
		if (strcmp(argv[i], "quiet") == 0) {
			quiet_fail++;
			quiet_succ++;
		}
		if (strcmp(argv[i], "quiet_fail") == 0) {
			quiet_fail++;
		}
		if (strcmp(argv[i], "quiet_success") == 0) {
			quiet_succ++;
		}
		if (strcmp(argv[i], "audit") == 0) {
			audit++;
		}
	}

	if (use_uid) {
		/* Get information about the user. */
		pwd = pam_modutil_getpwuid(pamh, getuid());
		if (pwd == NULL) {
			pam_syslog(pamh, LOG_ERR,
				   "error retrieving information about user %lu",
				   (unsigned long)getuid());
			return PAM_USER_UNKNOWN;
		}
		user = pwd->pw_name;
	} else {
		/* Get the user's name. */
		ret = pam_get_user(pamh, &user, NULL);
		if (ret != PAM_SUCCESS) {
			pam_syslog(pamh, LOG_NOTICE,
				   "cannot determine user name: %s",
				   pam_strerror(pamh, ret));
			return ret;
		}

		/* Postpone requesting password data until it is needed */
	}

	/* Walk the argument list. */
	count = 0;
	left = qual = right = NULL;
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0) {
			continue;
		}
		if (strcmp(argv[i], "use_uid") == 0) {
			continue;
		}
		if (strcmp(argv[i], "quiet") == 0) {
			continue;
		}
		if (strcmp(argv[i], "quiet_fail") == 0) {
			continue;
		}
		if (strcmp(argv[i], "quiet_success") == 0) {
			continue;
		}
		if (strcmp(argv[i], "audit") == 0) {
			continue;
		}
		if (left == NULL) {
			left = argv[i];
			continue;
		}
		if (qual == NULL) {
			qual = argv[i];
			continue;
		}
		if (right == NULL) {
			right = argv[i];
			if (right == NULL)
				continue;

			count++;
			ret = evaluate(pamh, debug,
				       left, qual, right,
				       &pwd, user);
			if (ret == PAM_USER_UNKNOWN && audit)
				pam_syslog(pamh, LOG_NOTICE,
					   "error retrieving information about user %s",
					   user);
			if (ret != PAM_SUCCESS) {
				if(!quiet_fail && ret != PAM_USER_UNKNOWN)
					pam_syslog(pamh, LOG_INFO,
						   "requirement \"%s %s %s\" "
						   "not met by user \"%s\"",
						   left, qual, right, user);
				left = qual = right = NULL;
				break;
			}
			else
				if(!quiet_succ)
					pam_syslog(pamh, LOG_INFO,
						   "requirement \"%s %s %s\" "
						   "was met by user \"%s\"",
						   left, qual, right, user);
			left = qual = right = NULL;
			continue;
		}
	}

	if (left || qual || right) {
		ret = PAM_SERVICE_ERR;
		pam_syslog(pamh, LOG_ERR,
			"incomplete condition detected");
	} else if (count == 0) {
		ret = PAM_SUCCESS;
		pam_syslog(pamh, LOG_INFO,
			"no condition detected; module succeeded");
	}

	return ret;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
               int argc UNUSED, const char **argv UNUSED)
{
	return PAM_IGNORE;
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
	return pam_succeed_if(pamh, argc, argv);
}

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
	return pam_succeed_if(pamh, argc, argv);
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
	return pam_succeed_if(pamh, argc, argv);
}

int
pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
	return pam_succeed_if(pamh, argc, argv);
}

int
pam_sm_chauthtok(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
	return pam_succeed_if(pamh, argc, argv);
}
