/* Taken from logdaemon-5.0, only minimal changes.  --marekm */
/*
 * SPDX-FileCopyrightText: 1990 - 1995, Wietse Venema.
 *
 * SPDX-License-Identifier: Unlicense
 */

/************************************************************************
* Copyright 1995 by Wietse Venema.  All rights reserved. Individual files
* may be covered by other copyrights (as noted in the file itself.)
*
* This material was originally written and compiled by Wietse Venema at
* Eindhoven University of Technology, The Netherlands, in 1990, 1991,
* 1992, 1993, 1994 and 1995.
*
* Redistribution and use in source and binary forms are permitted
* provided that this entire copyright notice is duplicated in all such
* copies.
*
* This software is provided "as is" and without any expressed or implied
* warranties, including, without limitation, the implied warranties of
* merchantibility and fitness for any particular purpose.
************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef USE_PAM
#ident "$Id$"

#include "prototypes.h"
    /*
     * This module implements a simple but effective form of login access
     * control based on login names and on host (or domain) names, internet
     * addresses (or network numbers), or on terminal line names in case of
     * non-networked logins. Diagnostics are reported through syslog(3).
     *
     * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
     */
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include <netdb.h>
#include <grp.h>
#ifdef PRIMARY_GROUP_MATCH
#include <pwd.h>
#endif
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* for inet_ntoa() */

#include "sizeof.h"

#if !defined(MAXHOSTNAMELEN) || (MAXHOSTNAMELEN < 64)
#undef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

 /* Path name of the access control file. */
#ifndef	TABLE
#define TABLE	"/etc/login.access"
#endif

/* Delimiters for fields and for lists of users, ttys or hosts. */
static char fs[] = ":";		/* field separator */
static char sep[] = ", \t";	/* list-element separator */

static bool list_match (char *list, const char *item, bool (*match_fn) (const char *, const char *));
static bool user_match (const char *tok, const char *string);
static bool from_match (const char *tok, const char *string);
static bool string_match (const char *tok, const char *string);
static const char *resolve_hostname (const char *string);

/* login_access - match username/group and host/tty with access control file */
int login_access (const char *user, const char *from)
{
	FILE *fp;
	char line[BUFSIZ];
	char *perm;		/* becomes permission field */
	char *users;		/* becomes list of login names */
	char *froms;		/* becomes list of terminals or hosts */
	bool match = false;

	/*
	 * Process the table one line at a time and stop at the first match.
	 * Blank lines and lines that begin with a '#' character are ignored.
	 * Non-comment lines are broken at the ':' character. All fields are
	 * mandatory. The first field should be a "+" or "-" character. A
	 * non-existing table means no access control.
	 */
	fp = fopen (TABLE, "r");
	if (NULL != fp) {
		int lineno = 0;	/* for diagnostics */
		while (   !match
		       && (fgets (line, sizeof (line), fp) == line)) {
			ptrdiff_t  end;
			lineno++;
			end = strlen (line) - 1;
			if (line[0] == '\0' || line[end] != '\n') {
				SYSLOG ((LOG_ERR,
					 "%s: line %d: missing newline or line too long",
					 TABLE, lineno));
				continue;
			}
			if (line[0] == '#') {
				continue;	/* comment line */
			}
			while (end > 0 && isspace (line[end - 1])) {
				end--;
			}
			line[end] = '\0';	/* strip trailing whitespace */
			if (line[0] == '\0') {	/* skip blank lines */
				continue;
			}
			if (   ((perm = strtok (line, fs)) == NULL)
			    || ((users = strtok (NULL, fs)) == NULL)
			    || ((froms = strtok (NULL, fs)) == NULL)
			    || (strtok (NULL, fs) != NULL)) {
				SYSLOG ((LOG_ERR,
					 "%s: line %d: bad field count",
					 TABLE, lineno));
				continue;
			}
			if (perm[0] != '+' && perm[0] != '-') {
				SYSLOG ((LOG_ERR,
					 "%s: line %d: bad first field",
					 TABLE, lineno));
				continue;
			}
			match = (   list_match (froms, from, from_match)
			         && list_match (users, user, user_match));
		}
		(void) fclose (fp);
	} else if (errno != ENOENT) {
		int err = errno;
		SYSLOG ((LOG_ERR, "cannot open %s: %s", TABLE, strerror (err)));
	}
	return (!match || (line[0] == '+'))?1:0;
}

/* list_match - match an item against a list of tokens with exceptions */
static bool list_match (char *list, const char *item, bool (*match_fn) (const char *, const char*))
{
	char *tok;
	bool match = false;

	/*
	 * Process tokens one at a time. We have exhausted all possible matches
	 * when we reach an "EXCEPT" token or the end of the list. If we do find
	 * a match, look for an "EXCEPT" list and recurse to determine whether
	 * the match is affected by any exceptions.
	 */
	for (tok = strtok (list, sep); tok != NULL; tok = strtok (NULL, sep)) {
		if (strcasecmp (tok, "EXCEPT") == 0) {	/* EXCEPT: give up */
			break;
		}
		match = (*match_fn) (tok, item);
		if (match) {
			break;
		}
	}

	/* Process exceptions to matches. */
	if (match) {
		while (   ((tok = strtok (NULL, sep)) != NULL)
		       && (strcasecmp (tok, "EXCEPT") != 0))
			/* VOID */ ;
		if (tok == 0 || !list_match (NULL, item, match_fn)) {
			return (match);
		}
	}
	return false;
}

/* myhostname - figure out local machine name */
static char *myhostname (void)
{
	static char name[MAXHOSTNAMELEN + 1] = "";

	if (name[0] == '\0') {
		gethostname (name, sizeof (name));
		name[MAXHOSTNAMELEN] = '\0';
	}
	return (name);
}

#if HAVE_INNETGR
/* netgroup_match - match group against machine or user */
static bool
netgroup_match (const char *group, const char *machine, const char *user)
{
	static char *mydomain = NULL;

	if (mydomain == NULL) {
		static char domain[MAXHOSTNAMELEN + 1];

		getdomainname (domain, MAXHOSTNAMELEN);
		mydomain = domain;
	}

	return (innetgr (group, machine, user, mydomain) != 0);
}
#endif

/* user_match - match a username against one token */
static bool user_match (const char *tok, const char *string)
{
	struct group *group;

#ifdef PRIMARY_GROUP_MATCH
	struct passwd *userinf;
#endif
	char *at;

	/*
	 * If a token has the magic value "ALL" the match always succeeds.
	 * Otherwise, return true if the token fully matches the username, or if
	 * the token is a group that contains the username.
	 */
	at = strchr (tok + 1, '@');
	if (NULL != at) {	/* split user@host pattern */
		*at = '\0';
		return (   user_match (tok, string)
		        && from_match (at + 1, myhostname ()));
#if HAVE_INNETGR
	} else if (tok[0] == '@') {	/* netgroup */
		return (netgroup_match (tok + 1, NULL, string));
#endif
	} else if (string_match (tok, string)) {	/* ALL or exact match */
		return true;
	/* local, no need for xgetgrnam */
	} else if ((group = getgrnam (tok)) != NULL) {	/* try group membership */
		int i;
		for (i = 0; NULL != group->gr_mem[i]; i++) {
			if (strcasecmp (string, group->gr_mem[i]) == 0) {
				return true;
			}
		}
#ifdef PRIMARY_GROUP_MATCH
		/*
		 * If the string is a user whose initial GID matches the token,
		 * accept it. May avoid excessively long lines in /etc/group.
		 * Radu-Adrian Feurdean <raf@licj.soroscj.ro>
		 *
		 * XXX - disabled by default for now.  Need to verify that
		 * getpwnam() doesn't have some nasty side effects.  --marekm
		 */
		/* local, no need for xgetpwnam */
		userinf = getpwnam (string);
		if (NULL != userinf) {
			if (userinf->pw_gid == group->gr_gid) {
				return true;
			}
		}
#endif
	}
	return false;
}

static const char *resolve_hostname (const char *string)
{
	int              gai_err;
	const char       *addr_str;
	struct addrinfo  *addrs;

	static char      host[MAXHOSTNAMELEN];

	gai_err = getaddrinfo(string, NULL, NULL, &addrs);
	if (gai_err != 0) {
		SYSLOG ((LOG_ERR, "getaddrinfo(%s): %s", string, gai_strerror(gai_err)));
		return string;
	}

	addr_str = host;
	gai_err = getnameinfo(addrs[0].ai_addr, addrs[0].ai_addrlen,
	                      host, NITEMS(host), NULL, 0, NI_NUMERICHOST);
	if (gai_err != 0) {
		SYSLOG ((LOG_ERR, "getnameinfo(%s): %s", string, gai_strerror(gai_err)));
		addr_str = string;
	}

	freeaddrinfo(addrs);
	return addr_str;
}

/* from_match - match a host or tty against a list of tokens */

static bool from_match (const char *tok, const char *string)
{
	size_t tok_len;

	/*
	 * If a token has the magic value "ALL" the match always succeeds. Return
	 * true if the token fully matches the string. If the token is a domain
	 * name, return true if it matches the last fields of the string. If the
	 * token has the magic value "LOCAL", return true if the string does not
	 * contain a "." character. If the token is a network number, return true
	 * if it matches the head of the string.
	 */
#if HAVE_INNETGR
	if (tok[0] == '@') {	/* netgroup */
		return (netgroup_match (tok + 1, string, NULL));
	} else
#endif
	if (string_match (tok, string)) {	/* ALL or exact match */
		return true;
	} else if (tok[0] == '.') {	/* domain: match last fields */
		size_t str_len;
		str_len = strlen (string);
		tok_len = strlen (tok);
		if (   (str_len > tok_len)
		    && (strcasecmp (tok, string + str_len - tok_len) == 0)) {
			return true;
		}
	} else if (strcasecmp (tok, "LOCAL") == 0) {	/* local: no dots */
		if (strchr (string, '.') == NULL) {
			return true;
		}
	} else if (   (tok[0] != '\0' && tok[(tok_len = strlen (tok)) - 1] == '.') /* network */
		   && (strncmp (tok, resolve_hostname (string), tok_len) == 0)) {
		return true;
	}
	return false;
}

/* string_match - match a string against one token */
static bool string_match (const char *tok, const char *string)
{

	/*
	 * If the token has the magic value "ALL" the match always succeeds.
	 * Otherwise, return true if the token fully matches the string.
	 */
	if (strcasecmp (tok, "ALL") == 0) {	/* all: always matches */
		return true;
	} else if (strcasecmp (tok, string) == 0) {	/* try exact match */
		return true;
	}
	return false;
}

#else				/* !USE_PAM */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* !USE_PAM */
