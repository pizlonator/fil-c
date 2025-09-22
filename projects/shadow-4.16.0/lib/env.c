/*
 * SPDX-FileCopyrightText: 1989 - 1992, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1999, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "prototypes.h"
#include "defines.h"
#include "shadowlog.h"
#include "string/sprintf.h"


/*
 * NEWENVP_STEP must be a power of two.  This is the number
 * of (char *) pointers to allocate at a time, to avoid using
 * realloc() too often.
 */
#define NEWENVP_STEP 16
size_t newenvc = 0;
/*@null@*/char **newenvp = NULL;

static const char *const forbid[] = {
	"_RLD_=",
	"BASH_ENV=",		/* GNU creeping featurism strikes again... */
	"ENV=",
	"HOME=",
	"IFS=",
	"KRB_CONF=",
	"LD_",			/* anything with the LD_ prefix */
	"LIBPATH=",
	"MAIL=",
	"NLSPATH=",
	"PATH=",
	"SHELL=",
	"SHLIB_PATH=",
	NULL
};

/* these are allowed, but with no slashes inside
   (to work around security problems in GNU gettext) */
static const char *const noslash[] = {
	"LANG=",
	"LANGUAGE=",
	"LC_",			/* anything with the LC_ prefix */
	NULL
};

/*
 * initenv() must be called once before using addenv().
 */
void initenv (void)
{
	newenvp = XMALLOC(NEWENVP_STEP, char *);
	*newenvp = NULL;
}


void addenv (const char *string, /*@null@*/const char *value)
{
	char    *cp, *newstring;
	size_t  i, n;

	if (NULL != value) {
		xasprintf(&newstring, "%s=%s", string, value);
	} else {
		newstring = xstrdup (string);
	}

	/*
	 * Search for a '=' character within the string and if none is found
	 * just ignore the whole string.
	 */

	cp = strchr (newstring, '=');
	if (NULL == cp) {
		free(newstring);
		return;
	}

	n = (size_t) (cp - newstring);

	/*
	 * If this environment variable is already set, change its value.
	 */
	for (i = 0; i < newenvc; i++) {
		if (   (strncmp (newstring, newenvp[i], n) == 0)
		    && (('=' == newenvp[i][n]) || ('\0' == newenvp[i][n]))) {
			break;
		}
	}

	if (i < newenvc) {
		free(newenvp[i]);
		newenvp[i] = newstring;
		return;
	}

	/*
	 * Otherwise, save the new environment variable
	 */
	newenvp[newenvc++] = newstring;

	/*
	 * And extend the environment if needed.
	 */

	/*
	 * Check whether newenvc is a multiple of NEWENVP_STEP.
	 * If so we have to resize the vector.
	 * the expression (newenvc & (NEWENVP_STEP - 1)) == 0
	 * is equal to    (newenvc %  NEWENVP_STEP) == 0
	 * as long as NEWENVP_STEP is a power of 2.
	 */

	if ((newenvc & (NEWENVP_STEP - 1)) == 0) {
		bool  update_environ;

		update_environ = (environ == newenvp);

		newenvp = XREALLOC(newenvp, newenvc + NEWENVP_STEP, char *);

		/*
		 * If this is our current environment, update
		 * environ so that it doesn't point to some
		 * free memory area (realloc() could move it).
		 */
		if (update_environ)
			environ = newenvp;
	}

	/*
	 * The last entry of newenvp must be NULL
	 */

	newenvp[newenvc] = NULL;
}


/*
 * set_env - copy command line arguments into the environment
 */
void set_env (int argc, char *const *argv)
{
	int   noname = 1;
	char  variable[1024];
	char  *cp;

	for (; argc > 0; argc--, argv++) {
		if (strlen (*argv) >= sizeof variable) {
			continue;	/* ignore long entries */
		}

		cp = strchr (*argv, '=');
		if (NULL == cp) {
			assert(SNPRINTF(variable, "L%d", noname) != -1);
			noname++;
			addenv (variable, *argv);
		} else {
			const char *const *p;

			for (p = forbid; NULL != *p; p++) {
				if (strncmp (*argv, *p, strlen (*p)) == 0) {
					break;
				}
			}

			if (NULL != *p) {
				stpcpy(mempcpy(variable, *argv, (size_t)(cp - *argv)), "");
				printf (_("You may not change $%s\n"),
					variable);
				continue;
			}

			addenv (*argv, NULL);
		}
	}
}

/*
 * sanitize_env - remove some nasty environment variables
 * If you fall into a total paranoia, you should call this
 * function for any root-setuid program or anything the user
 * might change the environment with. 99% useless as almost
 * all modern Unixes will handle setuid executables properly,
 * but... I feel better with that silly precaution. -j.
 */

void sanitize_env (void)
{
	char **envp = environ;
	const char *const *bad;
	char **cur;
	char **move;

	for (cur = envp; NULL != *cur; cur++) {
		for (bad = forbid; NULL != *bad; bad++) {
			if (strncmp (*cur, *bad, strlen (*bad)) == 0) {
				for (move = cur; NULL != *move; move++) {
					*move = *(move + 1);
				}
				cur--;
				break;
			}
		}
	}

	for (cur = envp; NULL != *cur; cur++) {
		for (bad = noslash; NULL != *bad; bad++) {
			if (strncmp (*cur, *bad, strlen (*bad)) != 0) {
				continue;
			}
			if (strchr (*cur, '/') == NULL) {
				continue;	/* OK */
			}
			for (move = cur; NULL != *move; move++) {
				*move = *(move + 1);
			}
			cur--;
			break;
		}
	}
}

