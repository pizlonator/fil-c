/*
 * pam_access module
 *
 * Written by Alexei Nogin <alexei@nogin.dnttm.ru> 1997/06/15
 * (I took login_access from logdaemon-5.6 and converted it to PAM
 * using parts of pam_time code.)
 *
 ************************************************************************
 * Copyright message from logdaemon-5.6 (original file name DISCLAIMER)
 ************************************************************************
 * Copyright 1995 by Wietse Venema. All rights reserved. Individual files
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
 * merchantability and fitness for any particular purpose.
 *************************************************************************
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <glob.h>
#ifdef HAVE_LIBAUDIT
#include <libaudit.h>
#endif

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_cc_compat.h"
#include "pam_inline.h"

#define PAM_ACCESS_CONFIG	(SCONFIG_DIR "/access.conf")
#define ACCESS_CONF_GLOB	(SCONFIG_DIR "/access.d/*.conf")
#ifdef VENDOR_SCONFIG_DIR
#define VENDOR_PAM_ACCESS_CONFIG (VENDOR_SCONFIG_DIR "/access.conf")
#define VENDOR_ACCESS_CONF_GLOB  (VENDOR_SCONFIG_DIR "/access.d/*.conf")
#endif

/* login_access.c from logdaemon-5.6 with several changes by A.Nogin: */

 /*
  * This module implements a simple but effective form of login access
  * control based on login names and on host (or domain) names, internet
  * addresses (or network numbers), or on terminal line names in case of
  * non-networked logins. Diagnostics are reported through syslog(3).
  *
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#if !defined(MAXHOSTNAMELEN) || (MAXHOSTNAMELEN < 64)
#undef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

 /* Delimiters for fields and for lists of users, ttys or hosts. */


#define ALL             2
#define YES             1
#define NO              0
#define NOMATCH       (-1)

 /*
  * A structure to bundle up all login-related information to keep the
  * functional interfaces as generic as possible.
  */
struct login_info {
    const struct passwd *user;
    const char *from;
    const char *config_file;
    const char *hostname;
    int debug;				/* Print debugging messages. */
    int only_new_group_syntax;		/* Only allow group entries of the form "(xyz)" */
    int noaudit;			/* Do not audit denials */
    int quiet_log;			/* Do not log denials */
    int nodns;                          /* Do not try to resolve tokens as hostnames */
    const char *fs;			/* field separator */
    const char *sep;			/* list-element separator */
    int from_remote_host;               /* If PAM_RHOST was used for from */
    struct addrinfo *res;		/* Cached DNS resolution of from */
    int gai_rv;				/* Cached retval of getaddrinfo */
};

/* Parse module config arguments */

static int
parse_args(pam_handle_t *pamh, struct login_info *loginfo,
           int argc, const char **argv)
{
    int i;

    loginfo->noaudit = NO;
    loginfo->quiet_log = NO;
    loginfo->debug = NO;
    loginfo->only_new_group_syntax = NO;
    loginfo->fs = ":";
    loginfo->sep = ", \t";
    for (i=0; i<argc; ++i) {
	const char *str;

	if ((str = pam_str_skip_prefix(argv[i], "fieldsep=")) != NULL) {

	    /* the admin wants to override the default field separators */
	    loginfo->fs = str;

	} else if ((str = pam_str_skip_prefix(argv[i], "listsep=")) != NULL) {

	    /* the admin wants to override the default list separators */
	    loginfo->sep = str;

	} else if ((str = pam_str_skip_prefix(argv[i], "accessfile=")) != NULL) {
	    FILE *fp = fopen(str, "r");

	    if (fp) {
		loginfo->config_file = str;
		fclose(fp);
	    } else {
		pam_syslog(pamh, LOG_ERR,
			   "failed to open accessfile=[%s]: %m", str);
		return 0;
	    }

	} else if (strcmp (argv[i], "debug") == 0) {
	    loginfo->debug = YES;
	} else if (strcmp (argv[i], "nodefgroup") == 0) {
	    loginfo->only_new_group_syntax = YES;
	} else if (strcmp (argv[i], "noaudit") == 0) {
	    loginfo->noaudit = YES;
	} else if (strcmp (argv[i], "quiet_log") == 0) {
	    loginfo->quiet_log = YES;
	} else if (strcmp (argv[i], "nodns") == 0) {
	    loginfo->nodns = YES;
	} else {
	    pam_syslog(pamh, LOG_ERR, "unrecognized option [%s]", argv[i]);
	}
    }

    return 1;  /* OK */
}

/* --- evaluating all files in VENDORDIR/security/access.d and /etc/security/access.d --- */
static const char *base_name(const char *path)
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
 * - If etc/security/access.d/@filename@.conf exists, then
 *   %vendordir%/security/access.d/@filename@.conf should not be used.
 * - All files in both access.d directories are sorted by their @filename@.conf in
 *   lexicographic order regardless of which of the directories they reside in. */
static char **read_access_dir(pam_handle_t *pamh)
{
	glob_t globbuf;
	size_t i=0;
	int glob_rv = glob(ACCESS_CONF_GLOB, GLOB_ERR | GLOB_NOSORT, NULL, &globbuf);
	char **file_list;
	size_t file_list_size = glob_rv == 0 ? globbuf.gl_pathc : 0;

#ifdef VENDOR_ACCESS_CONF_GLOB
	glob_t globbuf_vendor;
	int glob_rv_vendor = glob(VENDOR_ACCESS_CONF_GLOB, GLOB_ERR | GLOB_NOSORT, NULL, &globbuf_vendor);
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
#ifdef VENDOR_ACCESS_CONF_GLOB
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

/* --- static functions for checking whether the user should be let in --- */

typedef int match_func (pam_handle_t *, char *, struct login_info *);

static int list_match (pam_handle_t *, char *, char *, struct login_info *,
		       match_func *);
static int user_match (pam_handle_t *, char *, struct login_info *);
static int group_match (pam_handle_t *, char *, const char *, int);
static int from_match (pam_handle_t *, char *, struct login_info *);
static int remote_match (pam_handle_t *, char *, struct login_info *);
static int string_match (pam_handle_t *, const char *, const char *, int);
static int network_netmask_match (pam_handle_t *, const char *, const char *, struct login_info *);


/* isipaddr - find out if string provided is an IP address or not */

static int
isipaddr (const char *string, int *addr_type,
	  struct sockaddr_storage *addr)
{
  struct sockaddr_storage local_addr;
  int is_ip;

  /* We use struct sockaddr_storage addr because
   * struct in_addr/in6_addr is an integral part
   * of struct sockaddr and we doesn't want to
   * use its value.
   */

  if (addr == NULL)
    addr = &local_addr;

  memset(addr, 0, sizeof(struct sockaddr_storage));

  /* first ipv4 */
  if (inet_pton(AF_INET, string, addr) > 0)
    {
      if (addr_type != NULL)
	*addr_type = AF_INET;

      is_ip = YES;
    }
  else if (inet_pton(AF_INET6, string, addr) > 0)
    { /* then ipv6 */
      if (addr_type != NULL) {
	*addr_type = AF_INET6;
      }
      is_ip = YES;
    }
  else
    is_ip = NO;

  return is_ip;
}

/* is_local_addr - checks if the IP address is local */
static int
is_local_addr (const char *string, int addr_type)
{
  if (addr_type == AF_INET) {
    if (strcmp(string, "127.0.0.1") == 0) {
      return YES;
    }
  } else if (addr_type == AF_INET6) {
    if (strcmp(string, "::1") == 0) {
      return YES;
    }
  }

  return NO;
}


/* are_addresses_equal - translate IP address strings to real IP
 * addresses and compare them to find out if they are equal.
 * If netmask was provided it will be used to focus comparison to
 * relevant bits.
 */
static int
are_addresses_equal (const char *ipaddr0, const char *ipaddr1,
		     const char *netmask)
{
  struct sockaddr_storage addr0;
  struct sockaddr_storage addr1;
  int addr_type0 = 0;
  int addr_type1 = 0;

  if (isipaddr (ipaddr0, &addr_type0, &addr0) == NO)
    return NO;

  if (isipaddr (ipaddr1, &addr_type1, &addr1) == NO)
    return NO;

  if (addr_type0 != addr_type1) {
    /* different address types, but there is still a possibility that they are
     * both local addresses
     */
    int local1 = is_local_addr(ipaddr0, addr_type0);
    int local2 = is_local_addr(ipaddr1, addr_type1);

    if (local1 == YES && local2 == YES)
      return YES;

    return NO;
  }

  if (netmask != NULL) {
    /* Got a netmask, so normalize addresses? */
    struct sockaddr_storage nmask;
    unsigned char *byte_a, *byte_nm;

    memset(&nmask, 0, sizeof(struct sockaddr_storage));
    if (inet_pton(addr_type0, netmask, (void *)&nmask) > 0) {
      unsigned int i;
      byte_a = (unsigned char *)(&addr0);
      byte_nm = (unsigned char *)(&nmask);
      for (i=0; i<sizeof(struct sockaddr_storage); i++) {
        byte_a[i] = byte_a[i] & byte_nm[i];
      }

      byte_a = (unsigned char *)(&addr1);
      byte_nm = (unsigned char *)(&nmask);
      for (i=0; i<sizeof(struct sockaddr_storage); i++) {
        byte_a[i] = byte_a[i] & byte_nm[i];
      }
    }
  }


  /* Are the two addresses equal? */
  if (memcmp((void *)&addr0, (void *)&addr1,
              sizeof(struct sockaddr_storage)) == 0) {
    return(YES);
  }

  return(NO);
}

static char *
number_to_netmask (long netmask, int addr_type,
		   char *ipaddr_buf, size_t ipaddr_buf_len)
{
  /* We use struct sockaddr_storage addr because
   * struct in_addr/in6_addr is an integral part
   * of struct sockaddr and we doesn't want to
   * use its value.
   */
  struct sockaddr_storage nmask;
  unsigned char *byte_nm;
  const char *ipaddr_dst = NULL;
  int i, ip_bytes;

  if (netmask == 0) {
    /* mask 0 is the same like no mask */
    return(NULL);
  }

  memset(&nmask, 0, sizeof(struct sockaddr_storage));
  if (addr_type == AF_INET6) {
    /* ipv6 address mask */
    ip_bytes = 16;
  } else {
    /* default might be an ipv4 address mask */
    addr_type = AF_INET;
    ip_bytes = 4;
  }

  byte_nm = (unsigned char *)(&nmask);
  /* translate number to mask */
  for (i=0; i<ip_bytes; i++) {
    if (netmask >= 8) {
      byte_nm[i] = 0xff;
      netmask -= 8;
    } else
    if (netmask > 0) {
      byte_nm[i] = 0xff << (8 - netmask);
      break;
    } else
    if (netmask <= 0) {
      break;
    }
  }

  /* now generate netmask address string */
  ipaddr_dst = inet_ntop(addr_type, &nmask, ipaddr_buf, ipaddr_buf_len);
  if (ipaddr_dst == ipaddr_buf) {
    return (ipaddr_buf);
  }

  return (NULL);
}

/* login_access - match username/group and host/tty with access control file */

static int
login_access (pam_handle_t *pamh, struct login_info *item)
{
    FILE   *fp;
    char   *line = NULL;
    char   *perm;		/* becomes permission field */
    char   *users;		/* becomes list of login names */
    char   *froms;		/* becomes list of terminals or hosts */
    int     match = NO;
#ifdef HAVE_LIBAUDIT
    int     nonall_match = NO;
#endif
    int     result;
    size_t  end;
    size_t  lineno = 0;		/* for diagnostics */
    size_t  n = 0;
    char   *sptr;

    if (item->debug)
      pam_syslog (pamh, LOG_DEBUG,
		  "login_access: user=%s, from=%s, file=%s",
		  item->user->pw_name,
		  item->from, item->config_file);

    /*
     * Process the table one line at a time and stop at the first match.
     * Blank lines and lines that begin with a '#' character are ignored.
     * Non-comment lines are broken at the ':' character. All fields are
     * mandatory. The first field should be a "+" or "-" character. A
     * non-existing table means no access control.
     */

    if ((fp = fopen(item->config_file, "r"))!=NULL) {
	while (!match && getline(&line, &n, fp) != -1) {
	    lineno++;
	    if (line[0] == 0)
		continue;
	    if (line[end = strlen(line) - 1] != '\n') {
		pam_syslog(pamh, LOG_ERR,
                           "%s: line %zu: missing newline or line too long",
		           item->config_file, lineno);
		continue;
	    }
	    if (line[0] == '#')
		continue;			/* comment line */
	    while (end > 0 && isspace((unsigned char)line[end - 1]))
		end--;
	    line[end] = 0;			/* strip trailing whitespace */
	    if (line[0] == 0)			/* skip blank lines */
		continue;

	    /* Allow field separator in last field of froms */
	    if (!(perm = strtok_r(line, item->fs, &sptr))
		|| !(users = strtok_r(NULL, item->fs, &sptr))
		|| !(froms = strtok_r(NULL, "\n", &sptr))) {
		pam_syslog(pamh, LOG_ERR, "%s: line %zu: bad field count",
			   item->config_file, lineno);
		continue;
	    }
	    if (perm[0] != '+' && perm[0] != '-') {
		pam_syslog(pamh, LOG_ERR, "%s: line %zu: bad first field",
			   item->config_file, lineno);
		continue;
	    }
	    if (item->debug)
	      pam_syslog (pamh, LOG_DEBUG,
			  "line %zu: %s : %s : %s", lineno, perm, users, froms);
	    match = list_match(pamh, users, NULL, item, user_match);
	    if (item->debug)
	      pam_syslog (pamh, LOG_DEBUG, "user_match=%d, \"%s\"",
			  match, item->user->pw_name);
	    if (match) {
		match = list_match(pamh, froms, NULL, item, from_match);
#ifdef HAVE_LIBAUDIT
		if (!match && perm[0] == '+') {
		    nonall_match = YES;
		}
#endif
		if (item->debug)
		    pam_syslog (pamh, LOG_DEBUG,
				"from_match=%d, \"%s\"", match, item->from);
	    }
	}
	(void) fclose(fp);
    } else if (errno == ENOENT) {
        /* This is no error.  */
	pam_syslog(pamh, LOG_WARNING, "warning: cannot open %s: %m",
	           item->config_file);
    } else {
        pam_syslog(pamh, LOG_ERR, "cannot open %s: %m", item->config_file);
	return NO;
    }
#ifdef HAVE_LIBAUDIT
    if (!item->noaudit && (match == YES || (match == ALL &&
	nonall_match == YES)) && line != NULL && line[0] == '-') {
	pam_modutil_audit_write(pamh, AUDIT_ANOM_LOGIN_LOCATION,
	    "pam_access", 0);
    }
#endif
    if (match == NO)
	result = NOMATCH;
    else if (line != NULL && line[0] == '+')
	result = YES;
    else
	result = NO;
    free(line);
    return result;
}


/* list_match - match an item against a list of tokens with exceptions */

static int
list_match(pam_handle_t *pamh, char *list, char *sptr,
	   struct login_info *item, match_func *match_fn)
{
    char   *tok;
    int     match = NO;

    if (item->debug && list != NULL)
      pam_syslog (pamh, LOG_DEBUG,
		  "list_match: list=%s, item=%s", list, item->user->pw_name);

    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */

    for (tok = strtok_r(list, item->sep, &sptr); tok != NULL;
	 tok = strtok_r(NULL, item->sep, &sptr)) {
	if (strcasecmp(tok, "EXCEPT") == 0)	/* EXCEPT: give up */
	    break;
	if ((match = (*match_fn) (pamh, tok, item)))	/* YES */
	    break;
    }
    /* Process exceptions to matches. */

    if (match != NO) {
	while ((tok = strtok_r(NULL, item->sep, &sptr)) && strcasecmp(tok, "EXCEPT"))
	     /* VOID */ ;
	if (tok == NULL)
	    return match;
	if (list_match(pamh, NULL, sptr, item, match_fn) == NO)
	    return YES; /* drop special meaning of ALL */
    }
    return (NO);
}

/* netgroup_match - match group against machine or user */

static int
netgroup_match (pam_handle_t *pamh, const char *netgroup,
		const char *machine, const char *user, int debug)
{
  int retval;
  char *mydomain = NULL;

#ifdef HAVE_GETDOMAINNAME
  char domainname_res[256];

  if (getdomainname (domainname_res, sizeof (domainname_res)) == 0)
    {
      if (domainname_res[0] != '\0' && strcmp (domainname_res, "(none)") != 0)
        {
          mydomain = domainname_res;
        }
    }
#endif

#ifdef HAVE_INNETGR
  retval = innetgr (netgroup, machine, user, mydomain);
#else
  retval = 0;
  pam_syslog (pamh, LOG_ERR, "pam_access does not have netgroup support");
#endif
  if (debug == YES)
    pam_syslog (pamh, LOG_DEBUG,
		"netgroup_match: %d (netgroup=%s, machine=%s, user=%s, domain=%s)",
		retval, netgroup ? netgroup : "NULL",
		machine ? machine : "NULL",
		user ? user : "NULL", mydomain ? mydomain : "NULL");
  return retval;
}

/* user_name_or_uid_match - match a username or user uid against one token */
static int
user_name_or_uid_match(pam_handle_t *pamh, const char *tok,
		       const struct login_info *item)
{
    /* ALL or exact match of username */
    int rv = string_match(pamh, tok, item->user->pw_name, item->debug);
    if (rv != NO)
	return rv;

    if (tok[strspn(tok, "0123456789")] != '\0')
	return NO;

    char buf[sizeof(long long) * 3 + 1];
    pam_sprintf(buf, "%llu", zero_extend_signed_to_ull(item->user->pw_uid));
    if (item->debug)
	pam_syslog(pamh, LOG_DEBUG, "user_match: tok=%s, uid=%s", tok, buf);

    /* check for exact match of uid */
    return string_match (pamh, tok, buf, item->debug);
}

/* user_match - match a user against one token */

static int
user_match (pam_handle_t *pamh, char *tok, struct login_info *item)
{
    char   *string = item->user->pw_name;
    struct login_info fake_item;
    char   *at;
    int    rv;

    if (item->debug)
      pam_syslog (pamh, LOG_DEBUG,
		  "user_match: tok=%s, item=%s", tok, string);

    /*
     * If a token has the magic value "ALL" the match always succeeds.
     * Otherwise, return YES if the token fully matches the username, if the
     * token is a group that contains the username, or if the token is the
     * name of the user's primary group.
     */

    /* Try to split on a pattern (@*[^@]+)(@+.*) */
    for (at = tok; *at == '@'; ++at);

    if (tok[0] == '(' && tok[strlen(tok) - 1] == ')') {
      return (group_match (pamh, tok, string, item->debug));
    } else if ((at = strchr(at, '@')) != NULL) {
        /* split user@host pattern */
	if (item->hostname == NULL)
	    return NO;
	memcpy (&fake_item, item, sizeof(fake_item));
	fake_item.from = item->hostname;
	fake_item.gai_rv = 0;
	fake_item.res = NULL;
	fake_item.from_remote_host = 1; /* hostname should be resolvable */
	*at = 0;
	if (!user_match (pamh, tok, item))
		return NO;
	rv = from_match (pamh, at + 1, &fake_item);
	if (fake_item.gai_rv == 0 && fake_item.res)
		freeaddrinfo(fake_item.res);
	return rv;
    } else if (tok[0] == '@') {			/* netgroup */
	const char *hostname = NULL;
	if (tok[1] == '@') {			/* add hostname to netgroup match */
		if (item->hostname == NULL)
		    return NO;
		++tok;
		hostname = item->hostname;
	}
        return (netgroup_match (pamh, tok + 1, hostname, string, item->debug));
    } else if ((rv=user_name_or_uid_match(pamh, tok, item)) != NO) /* ALL or exact match */
      return rv;
    else if (item->only_new_group_syntax == NO &&
	     pam_modutil_user_in_group_nam_nam (pamh,
						item->user->pw_name, tok))
      /* try group membership */
      return YES;

    return NO;
}


/* group_name_or_gid_match - match a group name or group gid against one token */
static int
group_name_or_gid_match(pam_handle_t *pamh, const char *tok,
			const char *usr, int debug)
{
    /* check for exact match of group name */
    if (pam_modutil_user_in_group_nam_nam(pamh, usr, tok) != NO)
	return YES;

    if (tok[strspn(tok, "0123456789")] != '\0')
	return NO;

    char *endptr = NULL;
    errno = 0;
    unsigned long int ul = strtoul(tok, &endptr, 10);
    gid_t gid = (gid_t) ul;
    if (errno != 0
	|| tok == endptr
	|| *endptr != '\0'
	|| (unsigned long) zero_extend_signed_to_ull(gid) != ul) {
	return NO;
    }

    if (debug)
	pam_syslog(pamh, LOG_DEBUG, "group_match: user=%s, gid=%s", usr, tok);

    /* check for exact match of gid */
    return pam_modutil_user_in_group_nam_gid(pamh, usr, gid);
}

/* group_match - match a username against token named group */

static int
group_match (pam_handle_t *pamh, char *tok, const char* usr, int debug)
{
    if (debug)
        pam_syslog (pamh, LOG_DEBUG,
		    "group_match: grp=%s, user=%s", tok, usr);

    if (strlen(tok) < 3)
        return NO;

    /* token is received under the format '(...)' */
    tok++;
    tok[strlen(tok) - 1] = '\0';

    if (group_name_or_gid_match (pamh, tok, usr, debug))
        return YES;

    return NO;
}


/* from_match - match a host or tty against a list of tokens */

static int
from_match (pam_handle_t *pamh, char *tok, struct login_info *item)
{
    const char *string = item->from;
    int        rv;

    if (item->debug)
      pam_syslog (pamh, LOG_DEBUG,
		  "from_match: tok=%s, item=%s", tok, string);

    /*
     * If a token has the magic value "ALL" the match always succeeds. Return
     * YES if the token fully matches the string. If the token is a domain
     * name, return YES if it matches the last fields of the string. If the
     * token has the magic value "LOCAL", return YES if the from field was
     * not taken by PAM_RHOST. If the token is a network number, return YES
     * if it matches the head of the string.
     */

    if (string == NULL) {
	return NO;
    } else if (tok[0] == '@') {			/* netgroup */
        return (netgroup_match (pamh, tok + 1, string, (char *) 0, item->debug));
    } else if ((rv = string_match(pamh, tok, string, item->debug)) != NO) {
        /* ALL or exact match */
	return rv;
    } else if (strcasecmp(tok, "LOCAL") == 0) {
	    /* LOCAL matches only local accesses */
	    if (!item->from_remote_host)
	        return YES;
	    return NO;
    } else if (item->from_remote_host) {
        return remote_match(pamh, tok, item);
    }
    return NO;
}

static int
remote_match (pam_handle_t *pamh, char *tok, struct login_info *item)
{
    const char *string = item->from;
    size_t tok_len = strlen(tok);
    size_t str_len;

    if (tok[0] == '.') {			/* domain: match last fields */
      if ((str_len = strlen(string)) > tok_len
	  && strcasecmp(tok, string + str_len - tok_len) == 0)
	return YES;
    } else if (tok[tok_len - 1] == '.') {       /* internet network numbers/subnet (end with ".") */
      struct addrinfo hint;

      memset (&hint, '\0', sizeof (hint));
      hint.ai_flags = AI_CANONNAME;
      hint.ai_family = AF_INET;

      if (item->gai_rv != 0)
	return NO;
      else if (!item->res &&
		(item->gai_rv = getaddrinfo (string, NULL, &hint, &item->res)) != 0)
	return NO;
      else
	{
	  struct addrinfo *runp = item->res;

          while (runp != NULL)
	    {
	      char buf[INET_ADDRSTRLEN+2];

	      if (runp->ai_family == AF_INET)
		{
		  DIAG_PUSH_IGNORE_CAST_ALIGN;
		  inet_ntop (runp->ai_family,
			     &((struct sockaddr_in *) runp->ai_addr)->sin_addr,
			     buf, sizeof (buf) - 1);
		  DIAG_POP_IGNORE_CAST_ALIGN;

		  strcat (buf, ".");

		  if (strncmp(tok, buf, tok_len) == 0)
		    {
		      return YES;
		    }
		}
	      runp = runp->ai_next;
	    }
	}
      return NO;
    }

    /* Assume network/netmask, IP address or hostname.  */
    return network_netmask_match(pamh, tok, string, item);
}

/* string_match - match a string against one token */

static int
string_match (pam_handle_t *pamh, const char *tok, const char *string,
    int debug)
{

    if (debug)
        pam_syslog (pamh, LOG_DEBUG,
		    "string_match: tok=%s, item=%s", tok, string);

    /*
     * If the token has the magic value "ALL" the match always succeeds.
     * Otherwise, return YES if the token fully matches the string.
     * "NONE" token matches NULL string.
     */

    if (strcasecmp(tok, "ALL") == 0) {		/* all: always matches */
	return (ALL);
    } else if (string != NULL) {
	if (strcasecmp(tok, string) == 0) {	/* try exact match */
	    return (YES);
	}
    } else if (strcasecmp(tok, "NONE") == 0) {
	return (YES);
    }
    return (NO);
}


static int
is_device (pam_handle_t *pamh, const char *tok)
{
  struct stat st;
  const char *dev = "/dev/";
  char *devname;

  devname = malloc (strlen(dev) + strlen (tok) + 1);
  if (devname == NULL) {
      pam_syslog(pamh, LOG_ERR, "Cannot allocate memory for device name: %m");
      /*
       * We should return an error and abort, but pam_access has no good
       * error handling.
       */
      return NO;
  }

  char *cp = stpcpy (devname, dev);
  strcpy (cp, tok);

  if (lstat(devname, &st) != 0)
    {
      free (devname);
      return NO;
    }
  free (devname);

  if (S_ISCHR(st.st_mode))
    return YES;

  return NO;
}

/* network_netmask_match - match a string against one token
 * where string is a hostname or ip (v4,v6) address and tok
 * represents either a hostname, a single ip (v4,v6) address
 * or a network/netmask
 */
static int
network_netmask_match (pam_handle_t *pamh,
		       const char *tok, const char *string, struct login_info *item)
{
    char *netmask_ptr;
    char netmask_string[MAXHOSTNAMELEN + 1];
    int addr_type;
    struct addrinfo *ai = NULL;

    if (item->debug)
      pam_syslog (pamh, LOG_DEBUG,
		"network_netmask_match: tok=%s, item=%s", tok, string);

    /* OK, check if tok is of type addr/mask */
    if ((netmask_ptr = strchr(tok, '/')) != NULL)
      {
	long netmask = 0;

	/* YES */
	*netmask_ptr = 0;
	netmask_ptr++;

	if (isipaddr(tok, &addr_type, NULL) == NO)
	  { /* no netaddr */
	    return NO;
	  }

	/* check netmask */
	if (isipaddr(netmask_ptr, NULL, NULL) == NO)
	  { /* netmask as integre value */
	    char *endptr = NULL;
	    netmask = strtol(netmask_ptr, &endptr, 0);
	    if ((endptr == netmask_ptr) || (*endptr != '\0'))
		{ /* invalid netmask value */
		  return NO;
		}
	    if ((netmask < 0)
		|| (addr_type == AF_INET && netmask > 32)
		|| (addr_type == AF_INET6 && netmask > 128))
		{ /* netmask value out of range */
		  return NO;
		}

	    netmask_ptr = number_to_netmask(netmask, addr_type,
		netmask_string, MAXHOSTNAMELEN);
	  }

        /*
         * Construct an addrinfo list from the IP address.
         * This should not fail as the input is a correct IP address...
         */
	if (getaddrinfo (tok, NULL, NULL, &ai) != 0)
	  {
	    return NO;
	  }
      }
    else if (isipaddr(tok, NULL, NULL) == YES)
      {
	if (getaddrinfo (tok, NULL, NULL, &ai) != 0)
	  {
	    if (item->debug)
	      pam_syslog(pamh, LOG_DEBUG, "cannot resolve IP address \"%s\"", tok);

	    return NO;
	  }
	netmask_ptr = NULL;
      }
    else if (item->nodns)
      {
	/* Only hostnames are left, which we would need to resolve via DNS */
	return NO;
      }
    else
      {
	/* Bail out on X11 Display entries and ttys. */
	if (tok[0] == ':')
	  {
	    if (item->debug)
	      pam_syslog (pamh, LOG_DEBUG,
			  "network_netmask_match: tok=%s is X11 display", tok);
	    return NO;
	  }
	if (is_device (pamh, tok))
	  {
	    if (item->debug)
	      pam_syslog (pamh, LOG_DEBUG,
			  "network_netmask_match: tok=%s is a TTY", tok);
	    return NO;
	  }

        /*
	 * It is most likely a hostname.
	 * Let getaddrinfo sort everything out
	 */
	if (getaddrinfo (tok, NULL, NULL, &ai) != 0)
	  {
	    if (item->debug)
	      pam_syslog(pamh, LOG_DEBUG, "cannot resolve hostname \"%s\"", tok);

	    return NO;
	  }
	netmask_ptr = NULL;
      }

    if (isipaddr(string, NULL, NULL) != YES)
      {
	struct addrinfo hint;

	/* Assume network/netmask with a name of a host.  */
	memset (&hint, '\0', sizeof (hint));
	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = AF_UNSPEC;

	if (item->gai_rv != 0)
	  {
	    freeaddrinfo(ai);
	    return NO;
	  }
	else if (!item->res &&
		(item->gai_rv = getaddrinfo (string, NULL, &hint, &item->res)) != 0)
	  {
	    freeaddrinfo(ai);
	    return NO;
	  }
        else
	  {
	    struct addrinfo *runp = item->res;
	    struct addrinfo *runp1;

	    while (runp != NULL)
	      {
		char buf[INET6_ADDRSTRLEN];

		if (getnameinfo (runp->ai_addr, runp->ai_addrlen, buf, sizeof (buf), NULL, 0, NI_NUMERICHOST) != 0)
		  {
		    freeaddrinfo(ai);
		    return NO;
		  }

		for (runp1 = ai; runp1 != NULL; runp1 = runp1->ai_next)
		  {
                    char buf1[INET6_ADDRSTRLEN];

                    if (runp->ai_family != runp1->ai_family)
                      continue;

                    if (getnameinfo (runp1->ai_addr, runp1->ai_addrlen, buf1, sizeof (buf1), NULL, 0, NI_NUMERICHOST) != 0)
		      {
			freeaddrinfo(ai);
			return NO;
		      }

                    if (are_addresses_equal (buf, buf1, netmask_ptr))
                      {
                        freeaddrinfo(ai);
                        return YES;
                      }
		  }
		runp = runp->ai_next;
	      }
	  }
      }
    else
      {
       struct addrinfo *runp1;

       for (runp1 = ai; runp1 != NULL; runp1 = runp1->ai_next)
         {
           char buf1[INET6_ADDRSTRLEN];

           (void) getnameinfo (runp1->ai_addr, runp1->ai_addrlen, buf1, sizeof (buf1), NULL, 0, NI_NUMERICHOST);

           if (are_addresses_equal(string, buf1, netmask_ptr))
             {
               freeaddrinfo(ai);
               return YES;
             }
         }
      }

  freeaddrinfo(ai);

  return NO;
}


/* --- public PAM management functions --- */

static int
pam_access(pam_handle_t *pamh, int argc, const char **argv)
{
    struct login_info loginfo;
    const char *user=NULL;
    const void *void_from=NULL;
    const char *from;
    const char *default_config = PAM_ACCESS_CONFIG;
    struct passwd *user_pw;
    char hostname[MAXHOSTNAMELEN + 1];
    int rv;

    /* set username */

    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS) {
	pam_syslog(pamh, LOG_NOTICE, "cannot determine user name");
	return PAM_USER_UNKNOWN;
    }

    if ((user_pw=pam_modutil_getpwnam(pamh, user))==NULL)
      return (PAM_USER_UNKNOWN);

    /*
     * Bundle up the arguments to avoid unnecessary clumsiness later on.
     */
    memset(&loginfo, '\0', sizeof(loginfo));
    loginfo.user = user_pw;
    loginfo.config_file = default_config;

    /* parse the argument list */

    if (!parse_args(pamh, &loginfo, argc, argv)) {
	pam_syslog(pamh, LOG_ERR, "failed to parse the module arguments");
	return PAM_ABORT;
    }

#ifdef VENDOR_PAM_ACCESS_CONFIG
    if (loginfo.config_file == default_config) {
      /* Check whether PAM_ACCESS_CONFIG file is available.
       * If it does not exist, fall back to VENDOR_PAM_ACCESS_CONFIG file. */
      struct stat buffer;
      if (stat(loginfo.config_file, &buffer) != 0 && errno == ENOENT) {
	default_config = VENDOR_PAM_ACCESS_CONFIG;
	loginfo.config_file = default_config;
      }
    }
#endif

    /* remote host name */

    if (pam_get_item(pamh, PAM_RHOST, &void_from)
	!= PAM_SUCCESS) {
	pam_syslog(pamh, LOG_ERR, "cannot find the remote host name");
	return PAM_ABORT;
    }
    from = void_from;

    if ((from==NULL) || (*from=='\0')) {

        /* local login, set tty name */

        loginfo.from_remote_host = 0;

        if (pam_get_item(pamh, PAM_TTY, &void_from) != PAM_SUCCESS
            || void_from == NULL) {
            D(("PAM_TTY not set, probing stdin"));
	    from = ttyname(STDIN_FILENO);
	    if (from != NULL) {
	        if (pam_set_item(pamh, PAM_TTY, from) != PAM_SUCCESS)
	            pam_syslog(pamh, LOG_WARNING, "couldn't set tty name");
	    } else {
	      if (pam_get_item(pamh, PAM_SERVICE, &void_from) != PAM_SUCCESS
		  || void_from == NULL) {
		pam_syslog (pamh, LOG_ERR,
		     "cannot determine remote host, tty or service name");
		return PAM_ABORT;
	      }
	      from = void_from;
	      if (loginfo.debug)
		pam_syslog (pamh, LOG_DEBUG,
			    "cannot determine tty or remote hostname, using service %s",
			    from);
	    }
        }
	else
	  from = void_from;

	if (from[0] == '/') {   /* full path, remove device path.  */
	    const char *f;
	    from++;
	    if ((f = strchr(from, '/')) != NULL) {
		from = f + 1;
	    }
	}
    }
    else
      loginfo.from_remote_host = 1;

    loginfo.from = from;

    hostname[sizeof(hostname)-1] = '\0';
    if (gethostname(hostname, sizeof(hostname)-1) == 0)
	loginfo.hostname = hostname;
    else {
	pam_syslog (pamh, LOG_ERR, "gethostname failed: %m");
	loginfo.hostname = NULL;
    }

    rv = login_access(pamh, &loginfo);

    if (rv == NOMATCH && loginfo.config_file == default_config) {
        char **filename_list = read_access_dir(pamh);
        if (filename_list != NULL) {
            for (int i = 0; filename_list[i] != NULL; i++) {
                loginfo.config_file = filename_list[i];
                rv = login_access(pamh, &loginfo);
                if (rv != NOMATCH)
                    break;
            }
            for (int i = 0; filename_list[i] != NULL; i++)
                free(filename_list[i]);
            free(filename_list);
        }
    }

    if (loginfo.gai_rv == 0 && loginfo.res)
	freeaddrinfo(loginfo.res);

    if (rv) {
	return (PAM_SUCCESS);
    } else {
	if (!loginfo.quiet_log) {
	    pam_syslog(pamh, LOG_ERR,
	               "access denied for user `%s' from `%s'",user,from);
	}
	return (PAM_PERM_DENIED);
    }
}

int
pam_sm_setcred (pam_handle_t *pamh UNUSED, int flags UNUSED,
		int argc UNUSED, const char **argv UNUSED)
{
  return PAM_IGNORE;
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
  return pam_access(pamh, argc, argv);
}

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
  return pam_access(pamh, argc, argv);
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
  return pam_access(pamh, argc, argv);
}

int
pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
  return pam_access(pamh, argc, argv);
}

int
pam_sm_chauthtok(pam_handle_t *pamh, int flags UNUSED,
		 int argc, const char **argv)
{
  return pam_access(pamh, argc, argv);
}

/* end of module definition */
