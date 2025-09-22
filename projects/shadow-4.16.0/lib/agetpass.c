/*
 * SPDX-FileCopyrightText:  2022, Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 */


#include <config.h>

#include "agetpass.h"

#include <limits.h>
#include <readpassphrase.h>
#include <stdlib.h>
#include <string.h>

#ident "$Id$"

#include "alloc.h"

#if WITH_LIBBSD == 0
#include "freezero.h"
#endif /* WITH_LIBBSD */


/*
 * SYNOPSIS
 *	[[gnu::malloc(erase_pass)]]
 *	char *agetpass(const char *prompt);
 *	char *agetpass_stdin();
 *
 *	void erase_pass(char *pass);
 *
 * ARGUMENTS
 *   agetpass()
 *	prompt	String to be printed before reading a password.
 *
 *   erase_pass()
 *	pass	password previously returned by agetpass().
 *
 * DESCRIPTION
 *   agetpass()
 *	This function is very similar to getpass(3).  It has several
 *	advantages compared to getpass(3):
 *
 *	- Instead of using a static buffer, agetpass() allocates memory
 *	  through malloc(3).  This makes the function thread-safe, and
 *	  also reduces the visibility of the buffer.
 *
 *	- agetpass() doesn't reallocate internally.  Some
 *	  implementations of getpass(3), such as glibc, do that, as a
 *	  consequence of calling getline(3).  That's a bug in glibc,
 *	  which allows leaking prefixes of passwords in freed memory.
 *
 *	- agetpass() doesn't overrun the output buffer.  If the input
 *	  password is too long, it simply fails.  Some implementations
 *	  of getpass(3), share the same bug that gets(3) has.
 *
 *	As soon as possible, the password obtained from agetpass() be
 *	erased by calling erase_pass(), to avoid possibly leaking the
 *	password.
 *
 *   agetpass_stdin()
 *	This function is the same as previous one (agetpass). Just the
 *	password is read from stdin and terminal is not required.
 *
 *   erase_pass()
 *	This function first clears the password, by calling
 *	explicit_bzero(3) (or an equivalent call), and then frees the
 *	allocated memory by calling free(3).
 *
 *	NULL is a valid input pointer, and in such a case, this call is
 *	a no-op.
 *
 * RETURN VALUE
 *	agetpass() returns a newly allocated buffer containing the
 *	password on success.  On error, errno is set to indicate the
 *	error, and NULL is returned.
 *
 * ERRORS
 *   agetpass()
 *	This function may fail for any errors that malloc(3) or
 *	readpassphrase(3) may fail, and in addition it may fail for the
 *	following errors:
 *
 *	ENOBUFS
 *		The input password was longer than PASS_MAX.
 *
 * CAVEATS
 *	If a password is passed twice to erase_pass(), the behavior is
 *	undefined.
 */


static char *
agetpass_internal(const char *prompt, int flags)
{
	char    *pass;
	size_t  len;

	/*
	 * Since we want to support passwords upto PASS_MAX, we need
	 * PASS_MAX bytes for the password itself, and one more byte for
	 * the terminating '\0'.  We also want to detect truncation, and
	 * readpassphrase(3) doesn't detect it, so we need some trick.
	 * Let's add one more byte, and if the password uses it, it
	 * means the introduced password was longer than PASS_MAX.
	 */
	pass = MALLOC(PASS_MAX + 2, char);
	if (pass == NULL)
		return NULL;

	if (readpassphrase(prompt, pass, PASS_MAX + 2, flags) == NULL)
		goto fail;

	len = strlen(pass);
	if (len == PASS_MAX + 1) {
		errno = ENOBUFS;
		goto fail;
	}

	return pass;

fail:
	freezero(pass, PASS_MAX + 2);
	return NULL;
}

char *
agetpass(const char *prompt)
{
	return agetpass_internal(prompt, RPP_REQUIRE_TTY);
}

char *
agetpass_stdin()
{
	return agetpass_internal(NULL, RPP_STDIN);
}

void
erase_pass(char *pass)
{
	freezero(pass, PASS_MAX + 2);
}
