/*
 * Copyright 1989 - 1994, Julianne Frances Haugh
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Julianne F. Haugh nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JULIE HAUGH AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JULIE HAUGH OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <security/pam_modules.h>
#include <security/_pam_macros.h>


#include "pam_i18n.h"
#include "support.h"

/* can't be a palindrome - like `R A D A R' or `M A D A M' */
static int palindrome(const char *old, const char *new) {
	int	i, j;

	i = strlen (new);

	for (j = 0;j < i;j++)
		if (new[i - j - 1] != new[j])
			return 0;

	return 1;
}

/* more than half of the characters are different ones. */
static int similar(const char *old, const char *new) {
	int i, j;

	/*
	 * XXX - sometimes this fails when changing from a simple password
	 * to a really long one (MD5).  For now, I just return success if
	 * the new password is long enough.  Please feel free to suggest
	 * something better...  --marekm
	 */
	if (strlen(new) >= 8)
		return 0;

	for (i = j = 0; new[i] && old[i]; i++)
		if (strchr(new, old[i]))
			j++;

	if (i >= j * 2)
		return 0;

	return 1;
}

/* a nice mix of characters. */
static int simple(const char *old, const char *new) {
	int	digits = 0;
	int	uppers = 0;
	int	lowers = 0;
	int	others = 0;
	int	size;
	int	i;

	for (i = 0;new[i];i++) {
		if (isdigit (new[i]))
			digits++;
		else if (isupper (new[i]))
			uppers++;
		else if (islower (new[i]))
			lowers++;
		else
			others++;
	}

	/*
	 * The scam is this - a password of only one character type
	 * must be 8 letters long.  Two types, 7, and so on.
	 */

	size = 9;
	if (digits) size--;
	if (uppers) size--;
	if (lowers) size--;
	if (others) size--;

	if (size <= i)
		return 0;

	return 1;
}

static char *str_lower(char *string) {
	char *cp;

	for (cp = string; *cp; cp++)
		*cp = tolower(*cp);
	return string;
}

static const char * password_check(const char *old, const char *new,
				   const struct passwd *pwdp) {
	const char *msg = NULL;
	char *oldmono, *newmono, *wrapped;

	if (strcmp(new, old) == 0)
		return _("Bad: new password must be different than the old one");

	newmono = str_lower(strdup(new));
	oldmono = str_lower(strdup(old));
	wrapped = (char *)malloc(strlen(oldmono) * 2 + 1);
	strcpy (wrapped, oldmono);
	strcat (wrapped, oldmono);

	if (palindrome(oldmono, newmono)) {
		msg = _("Bad: new password cannot be a palindrome");
	} else if (strcmp(oldmono, newmono) == 0) {
		msg = _("Bad: new and old password must differ by more than just case");
	} else if (similar(oldmono, newmono)) {
		msg = _("Bad: new and old password are too similar");
	} else if (simple(old, new)) {
		msg = _("Bad: new password is too simple");
	} else if (strstr(wrapped, newmono)) {
		msg = _("Bad: new password is just a wrapped version of the old one");
	}

	_pam_delete(newmono);
	_pam_delete(oldmono);
	_pam_delete(wrapped);

	return msg;
}

const char *obscure_msg(const char *old, const char *new,
			       const struct passwd *pwdp, unsigned int ctrl) {
	int oldlen, newlen;
	char *new1, *old1;
	const char *msg;

	if (old == NULL)
		return NULL; /* no check if old is NULL */

	oldlen = strlen(old);
	newlen = strlen(new);

	/* Remaining checks are optional. */
	if (off(UNIX_OBSCURE_CHECKS,ctrl))
		return NULL;

	if ((msg = password_check(old, new, pwdp)) != NULL)
		return msg;

	/* The traditional crypt() truncates passwords to 8 chars.  It is
	   possible to circumvent the above checks by choosing an easy
	   8-char password and adding some random characters to it...
	   Example: "password$%^&*123".  So check it again, this time
	   truncated to the maximum length.  Idea from npasswd.  --marekm */

	if (!UNIX_DES_CRYPT(ctrl))
		return NULL;  /* unlimited password length */

	if (oldlen <= 8 && newlen <= 8)
		return NULL;

	new1 = strndup(new,8);
	old1 = strndup(old,8);

	msg = password_check(old1, new1, pwdp);

	_pam_delete(new1);
	_pam_delete(old1);

	return msg;
}
