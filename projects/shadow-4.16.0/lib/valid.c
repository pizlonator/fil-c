/*
 * SPDX-FileCopyrightText: 1989 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1999, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2008, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <sys/types.h>
#include <stdio.h>
#include "prototypes.h"
#include "defines.h"
#include <pwd.h>
/*
 * valid - compare encrypted passwords
 *
 *	Valid() compares the DES encrypted password from the password file
 *	against the password which the user has entered after it has been
 *	encrypted using the same salt as the original.  Entries which do
 *	not have a password file entry have a NULL pw_name field and this
 *	is used to indicate that a dummy salt must be used to encrypt the
 *	password anyway.
 */
bool valid (const char *password, const struct passwd *ent)
{
	const char *encrypted;
	/*@observer@*/const char *salt;

	/*
	 * Start with blank or empty password entries.  Always encrypt
	 * a password if no such user exists.  Only if the ID exists and
	 * the password is really empty do you return quickly.  This
	 * routine is meant to waste CPU time.
	 */

	if ((NULL != ent->pw_name) && ('\0' == ent->pw_passwd[0])) {
		if ('\0' == password[0]) {
			return true;	/* user entered nothing */
		} else {
			return false;	/* user entered something! */
		}
	}

	/*
	 * If there is no entry then we need a salt to use.
	 */

	if ((NULL == ent->pw_name) || ('\0' == ent->pw_passwd[0])) {
		salt = "xx";
	} else {
		salt = ent->pw_passwd;
	}

	/*
	 * Now, perform the encryption using the salt from before on
	 * the users input.  Since we always encrypt the string, it
	 * should be very difficult to determine if the user exists by
	 * looking at execution time.
	 */

	encrypted = pw_encrypt (password, salt);

	/*
	 * One last time we must deal with there being no password file
	 * entry for the user.  We use the pw_name == NULL idiom to
	 * cause non-existent users to not be validated.
	 */

	if (   (NULL != ent->pw_name)
	    && (NULL != encrypted)
	    && (strcmp (encrypted, ent->pw_passwd) == 0)) {
		return true;
	} else {
		return false;
	}
}

