/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1999, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"


#include <ctype.h>
#include <stdio.h>

#include "alloc.h"
#include "attr.h"
#include "memzero.h"
#include "prototypes.h"
#include "defines.h"
#include "getdef.h"

#if WITH_LIBBSD == 0
#include "freezero.h"
#endif /* WITH_LIBBSD */

/*
 * can't be a palindrome - like `R A D A R' or `M A D A M'
 */
static bool palindrome (MAYBE_UNUSED const char *old, const char *new)
{
	size_t i, j;

	i = strlen (new);

	for (j = 0; j < i; j++) {
		if (new[i - j - 1] != new[j]) {
			return false;
		}
	}

	return true;
}

/*
 * more than half of the characters are different ones.
 */

static bool similar (/*@notnull@*/const char *old, /*@notnull@*/const char *new)
{
	int i, j;

	/*
	 * XXX - sometimes this fails when changing from a simple password
	 * to a really long one (MD5).  For now, I just return success if
	 * the new password is long enough.  Please feel free to suggest
	 * something better...  --marekm
	 */
	if (strlen (new) >= 8) {
		return false;
	}

	for (i = j = 0; ('\0' != new[i]) && ('\0' != old[i]); i++) {
		if (strchr (new, old[i]) != NULL) {
			j++;
		}
	}

	if (i >= j * 2) {
		return false;
	}

	return true;
}

static char *str_lower (/*@returned@*/char *string)
{
	char *cp;

	for (cp = string; '\0' != *cp; cp++) {
		*cp = tolower (*cp);
	}
	return string;
}

static /*@observer@*//*@null@*/const char *password_check (
	/*@notnull@*/const char *old,
	/*@notnull@*/const char *new,
	/*@notnull@*/MAYBE_UNUSED const struct passwd *pwdp)
{
	const char *msg = NULL;
	char *oldmono, *newmono, *wrapped;

	if (strcmp (new, old) == 0) {
		return _("no change");
	}

	newmono = str_lower (xstrdup (new));
	oldmono = str_lower (xstrdup (old));
	wrapped = XMALLOC(strlen(oldmono) * 2 + 1, char);
	strcpy (wrapped, oldmono);
	strcat (wrapped, oldmono);

	if (palindrome (oldmono, newmono)) {
		msg = _("a palindrome");
	} else if (strcmp (oldmono, newmono) == 0) {
		msg = _("case changes only");
	} else if (similar (oldmono, newmono)) {
		msg = _("too similar");
	} else if (strstr (wrapped, newmono) != NULL) {
		msg = _("rotated");
	}
	strzero (newmono);
	strzero (oldmono);
	strzero (wrapped);
	free (newmono);
	free (oldmono);
	free (wrapped);

	return msg;
}

static /*@observer@*//*@null@*/const char *obscure_msg (
	/*@notnull@*/const char *old,
	/*@notnull@*/const char *new,
	/*@notnull@*/const struct passwd *pwdp)
{
	size_t maxlen, oldlen, newlen;
	char *new1, *old1;
	const char *msg;
	const char *result;

	oldlen = strlen (old);
	newlen = strlen (new);

	if (newlen < (size_t) getdef_num ("PASS_MIN_LEN", 0)) {
		return _("too short");
	}

	/*
	 * Remaining checks are optional.
	 */
	if (!getdef_bool ("OBSCURE_CHECKS_ENAB")) {
		return NULL;
	}

	msg = password_check (old, new, pwdp);
	if (NULL != msg) {
		return msg;
	}

	result = getdef_str ("ENCRYPT_METHOD");
	if (NULL == result) {
	/* The traditional crypt() truncates passwords to 8 chars.  It is
	   possible to circumvent the above checks by choosing an easy
	   8-char password and adding some random characters to it...
	   Example: "password$%^&*123".  So check it again, this time
	   truncated to the maximum length.  Idea from npasswd.  --marekm */

		if (getdef_bool ("MD5_CRYPT_ENAB")) {
			return NULL;
		}

	} else {

		if (   (strcmp (result, "MD5")    == 0)
#ifdef USE_SHA_CRYPT
		    || (strcmp (result, "SHA256") == 0)
		    || (strcmp (result, "SHA512") == 0)
#endif
#ifdef USE_BCRYPT
		    || (strcmp (result, "BCRYPT") == 0)
#endif
#ifdef USE_YESCRYPT
		    || (strcmp (result, "YESCRYPT") == 0)
#endif
		    ) {
			return NULL;
		}

	}
	maxlen = getdef_num ("PASS_MAX_LEN", 8);
	if (   (oldlen <= maxlen)
	    && (newlen <= maxlen)) {
		return NULL;
	}

	new1 = xstrdup (new);
	old1 = xstrdup (old);
	if (newlen > maxlen) {
		new1[maxlen] = '\0';
	}
	if (oldlen > maxlen) {
		old1[maxlen] = '\0';
	}

	msg = password_check (old1, new1, pwdp);

	freezero (new1, newlen);
	freezero (old1, oldlen);

	return msg;
}

/*
 * Obscure - see if password is obscure enough.
 *
 *	The programmer is encouraged to add as much complexity to this
 *	routine as desired.  Included are some of my favorite ways to
 *	check passwords.
 */

bool obscure (const char *old, const char *new, const struct passwd *pwdp)
{
	const char *msg = obscure_msg (old, new, pwdp);

	if (NULL != msg) {
		printf (_("Bad password: %s.  "), msg);
		return false;
	}
	return true;
}
