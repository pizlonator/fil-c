/*
 * This file implements the following functions:
 *   pam_modutil_search_key:
 *     lookup a value for key in login.defs file or similar key value format
 */

#include "config.h"

#include "pam_private.h"
#include "pam_modutil_private.h"
#include <security/pam_ext.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef USE_ECONF
#include <libeconf.h>
#endif

#ifdef USE_ECONF
#define LOGIN_DEFS "/etc/login.defs"

#ifndef VENDORDIR
#define VENDORDIR NULL
#endif

static char *
econf_search_key (const char *name, const char *suffix, const char *key)
{
	econf_file *key_file = NULL;
	char *val;

	if (econf_readDirs (&key_file, VENDORDIR, SYSCONFDIR, name, suffix,
			    " \t", "#"))
		return NULL;

	if (econf_getStringValue (key_file, NULL, key, &val)) {
		econf_free (key_file);
		return NULL;
	}

	econf_free (key_file);

	return val;
}

#endif

/* lookup a value for key in login.defs file or similar key value format */
char *
pam_modutil_search_key(pam_handle_t *pamh UNUSED,
		       const char *file_name,
		       const char *key)
{
	FILE *fp;
	char *buf = NULL;
	size_t buflen = 0;
	char *retval = NULL;

#ifdef USE_ECONF
	if (strcmp (file_name, LOGIN_DEFS) == 0)
		return econf_search_key ("login", ".defs", key);
#endif

	fp = fopen(file_name, "r");
	if (NULL == fp)
		return NULL;

	while (!feof(fp)) {
		char *tmp, *cp;
		ssize_t n = getline(&buf, &buflen, fp);

		cp = buf;

		if (n < 1)
			break;
		if (cp[n - 1] == '\n')
			cp[n - 1] = '\0';

		tmp = strchr(cp, '#');  /* remove comments */
		if (tmp)
			*tmp = '\0';
		while (isspace((unsigned char)*cp))    /* remove spaces and tabs */
			++cp;
		if (*cp == '\0')        /* ignore empty lines */
			continue;

		tmp = strsep (&cp, " \t=");
		if (cp != NULL)
			while (isspace((unsigned char)*cp) || *cp == '=')
				++cp;
		else
			cp = buf + n;   /* empty string */

		if (strcasecmp(tmp, key) == 0) {
			retval = strdup(cp);
			break;
		}
	}
	fclose(fp);

	free(buf);

	return retval;
}
