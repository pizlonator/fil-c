/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2009       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

/* Newer versions of Linux libc already have shadow support.  */
#ifndef HAVE_GETSPNAM

#ident "$Id$"

#include <sys/types.h>
#include "prototypes.h"
#include "defines.h"
#include <stdio.h>

#include "atoi/str2i.h"


static FILE *shadow;

#define	FIELDS	9
#define	OFIELDS	5


/*
 * setspent - initialize access to shadow text and DBM files
 */

void setspent (void)
{
	if (NULL != shadow) {
		rewind (shadow);
	}else {
		shadow = fopen (SHADOW_FILE, "r");
	}
}

/*
 * endspent - terminate access to shadow text and DBM files
 */

void endspent (void)
{
	if (NULL != shadow) {
		(void) fclose (shadow);
	}

	shadow = NULL;
}

/*
 * my_sgetspent - convert string in shadow file format to (struct spwd *)
 */

static struct spwd *my_sgetspent (const char *string)
{
	static char spwbuf[BUFSIZ];
	static struct spwd spwd;
	char *fields[FIELDS];
	char *cp;
	int i;

	/*
	 * Copy string to local buffer.  It has to be tokenized and we
	 * have to do that to our private copy.
	 */

	if (strlen (string) >= sizeof spwbuf)
		return 0;
	strcpy (spwbuf, string);

	cp = strrchr (spwbuf, '\n');
	if (NULL != cp)
		*cp = '\0';

	/*
	 * Tokenize the string into colon separated fields.  Allow up to
	 * FIELDS different fields.
	 */

	for (cp = spwbuf, i = 0; *cp && i < FIELDS; i++) {
		fields[i] = cp;
		while (*cp && *cp != ':')
			cp++;

		if (*cp)
			*cp++ = '\0';
	}

	if (i == (FIELDS - 1))
		fields[i++] = cp;

	if ((cp && *cp) || (i != FIELDS && i != OFIELDS))
		return 0;

	/*
	 * Start populating the structure.  The fields are all in
	 * static storage, as is the structure we pass back.  If we
	 * ever see a name with '+' as the first character, we try
	 * to turn on NIS processing.
	 */

	spwd.sp_namp = fields[0];
	spwd.sp_pwdp = fields[1];

	/*
	 * Get the last changed date.  For all of the integer fields,
	 * we check for proper format.  It is an error to have an
	 * incorrectly formatted number, unless we are using NIS.
	 */

	if (fields[2][0] == '\0') {
		spwd.sp_lstchg = -1;
	} else {
		if (str2sl(&spwd.sp_lstchg, fields[2]) == -1)
			return 0;
		if (spwd.sp_lstchg < 0)
			return 0;
	}

	/*
	 * Get the minimum period between password changes.
	 */

	if (fields[3][0] == '\0') {
		spwd.sp_min = -1;
	} else {
		if (str2sl(&spwd.sp_min, fields[3]) == -1)
			return 0;
		if (spwd.sp_min < 0)
			return 0;
	}

	/*
	 * Get the maximum number of days a password is valid.
	 */

	if (fields[4][0] == '\0') {
		spwd.sp_max = -1;
	} else {
		if (str2sl(&spwd.sp_max, fields[4]) == -1)
			return 0;
		if (spwd.sp_max < 0)
			return 0;
	}

	/*
	 * If there are only OFIELDS fields (this is a SVR3.2 /etc/shadow
	 * formatted file), initialize the other field members to -1.
	 */

	if (i == OFIELDS) {
		spwd.sp_warn   = -1;
		spwd.sp_inact  = -1;
		spwd.sp_expire = -1;
		spwd.sp_flag   = SHADOW_SP_FLAG_UNSET;

		return &spwd;
	}

	/*
	 * Get the number of days of password expiry warning.
	 */

	if (fields[5][0] == '\0') {
		spwd.sp_warn = -1;
	} else {
		if (str2sl(&spwd.sp_warn, fields[5]) == -1)
			return 0;
		if (spwd.sp_warn < 0)
			return 0;
	}

	/*
	 * Get the number of days of inactivity before an account is
	 * disabled.
	 */

	if (fields[6][0] == '\0') {
		spwd.sp_inact = -1;
	} else {
		if (str2sl(&spwd.sp_inact, fields[6]) == -1)
			return 0;
		if (spwd.sp_inact < 0)
			return 0;
	}

	/*
	 * Get the number of days after the epoch before the account is
	 * set to expire.
	 */

	if (fields[7][0] == '\0') {
		spwd.sp_expire = -1;
	} else {
		if (str2sl(&spwd.sp_expire, fields[7]) == -1)
			return 0;
		if (spwd.sp_expire < 0)
			return 0;
	}

	/*
	 * This field is reserved for future use.  But it isn't supposed
	 * to have anything other than a valid integer in it.
	 */

	if (fields[8][0] == '\0') {
		spwd.sp_flag = SHADOW_SP_FLAG_UNSET;
	} else {
		if (str2ul(&spwd.sp_flag, fields[8]) == -1)
			return 0;
		if (spwd.sp_flag < 0)
			return 0;
	}

	return (&spwd);
}

/*
 * fgetspent - get an entry from a /etc/shadow formatted stream
 */

struct spwd *fgetspent (FILE * fp)
{
	char buf[BUFSIZ];
	char *cp;

	if (NULL == fp) {
		return (0);
	}

	if (fgets (buf, sizeof buf, fp) != NULL)
	{
		cp = strchr (buf, '\n');
		if (NULL != cp) {
			*cp = '\0';
		}
		return my_sgetspent (buf);
	}
	return 0;
}

/*
 * getspent - get a (struct spwd *) from the current shadow file
 */

struct spwd *getspent (void)
{
	if (NULL == shadow) {
		setspent ();
	}
	return (fgetspent (shadow));
}

/*
 * getspnam - get a shadow entry by name
 */

struct spwd *getspnam (const char *name)
{
	struct spwd *sp;

	setspent ();

	while ((sp = getspent ()) != NULL) {
		if (strcmp (name, sp->sp_namp) == 0) {
			break;
		}
	}
	endspent ();
	return (sp);
}
#else
extern int ISO_C_forbids_an_empty_translation_unit;
#endif
