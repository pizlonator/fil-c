/*
 * SPDX-FileCopyrightText:  Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>
 * SPDX-FileCopyrightText:  J.T. Conklin <jtc@netbsd.org>
 *
 * SPDX-License-Identifier: Unlicense
 */

/*
 * salt.c - generate a random salt string for crypt()
 *
 * Written by Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>,
 * it is in the public domain.
 */

#include <config.h>

#ident "$Id$"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "prototypes.h"
#include "defines.h"
#include "getdef.h"
#include "shadowlog.h"

#if (defined CRYPT_GENSALT_IMPLEMENTS_AUTO_ENTROPY && \
     CRYPT_GENSALT_IMPLEMENTS_AUTO_ENTROPY)
#define USE_XCRYPT_GENSALT 1
#else
#define USE_XCRYPT_GENSALT 0
#endif

/* Add the salt prefix. */
#define MAGNUM(array,ch)	(array)[0]=(array)[2]='$',(array)[1]=(ch),(array)[3]='\0'

#ifdef USE_BCRYPT
/* Use $2b$ as prefix for compatibility with OpenBSD's bcrypt. */
#define BCRYPTMAGNUM(array)	(array)[0]=(array)[3]='$',(array)[1]='2',(array)[2]='b',(array)[4]='\0'
#define BCRYPT_SALT_SIZE 22
/* Default number of rounds if not explicitly specified.  */
#define B_ROUNDS_DEFAULT 13
/* Minimum number of rounds.  */
#define B_ROUNDS_MIN 4
/* Maximum number of rounds.  */
#define B_ROUNDS_MAX 31
#endif /* USE_BCRYPT */

#ifdef USE_SHA_CRYPT
/* Fixed salt len for sha{256,512}crypt. */
#define SHA_CRYPT_SALT_SIZE 16
/* Default number of rounds if not explicitly specified.  */
#define SHA_ROUNDS_DEFAULT 5000
/* Minimum number of rounds.  */
#define SHA_ROUNDS_MIN 1000
/* Maximum number of rounds.  */
#define SHA_ROUNDS_MAX 999999999
#endif

#ifdef USE_YESCRYPT
/*
 * Default number of base64 characters used for the salt.
 * 24 characters gives a 144 bits (18 bytes) salt. Unlike the more
 * traditional 128 bits (16 bytes) salt, this 144 bits salt is always
 * represented by the same number of base64 characters without padding
 * issue, even with a non-standard base64 encoding scheme.
 */
#define YESCRYPT_SALT_SIZE 24
/* Default cost if not explicitly specified.  */
#define Y_COST_DEFAULT 5
/* Minimum cost.  */
#define Y_COST_MIN 1
/* Maximum cost.  */
#define Y_COST_MAX 11
#endif

/* Fixed salt len for md5crypt. */
#define MD5_CRYPT_SALT_SIZE 8

/* Generate salt of size salt_size. */
#define MAX_SALT_SIZE 44
#define MIN_SALT_SIZE 8

/* Maximum size of the generated salt string. */
#define GENSALT_SETTING_SIZE 100

/* local function prototypes */
#if !USE_XCRYPT_GENSALT
static /*@observer@*/const char *gensalt (size_t salt_size);
#endif /* !USE_XCRYPT_GENSALT */
#ifdef USE_SHA_CRYPT
static /*@observer@*/unsigned long SHA_get_salt_rounds (/*@null@*/const int *prefered_rounds);
static /*@observer@*/void SHA_salt_rounds_to_buf (char *buf, unsigned long rounds);
#endif /* USE_SHA_CRYPT */
#ifdef USE_BCRYPT
static /*@observer@*/unsigned long BCRYPT_get_salt_rounds (/*@null@*/const int *prefered_rounds);
static /*@observer@*/void BCRYPT_salt_rounds_to_buf (char *buf, unsigned long rounds);
#endif /* USE_BCRYPT */
#ifdef USE_YESCRYPT
static /*@observer@*/unsigned long YESCRYPT_get_salt_cost (/*@null@*/const int *prefered_cost);
static /*@observer@*/void YESCRYPT_salt_cost_to_buf (char *buf, unsigned long cost);
#endif /* USE_YESCRYPT */


#ifdef USE_SHA_CRYPT
/* Return the the rounds number for the SHA crypt methods. */
static /*@observer@*/unsigned long SHA_get_salt_rounds (/*@null@*/const int *prefered_rounds)
{
	unsigned long rounds;

	if (NULL == prefered_rounds) {
		long min_rounds = getdef_long ("SHA_CRYPT_MIN_ROUNDS", -1);
		long max_rounds = getdef_long ("SHA_CRYPT_MAX_ROUNDS", -1);

		if ((-1 == min_rounds) && (-1 == max_rounds)) {
			rounds = SHA_ROUNDS_DEFAULT;
		}
		else {
			if (-1 == min_rounds) {
				min_rounds = max_rounds;
			}

			if (-1 == max_rounds) {
				max_rounds = min_rounds;
			}

			if (min_rounds > max_rounds) {
				max_rounds = min_rounds;
			}

			rounds = csrand_interval (min_rounds, max_rounds);
		}
	} else if (0 == *prefered_rounds) {
		rounds = SHA_ROUNDS_DEFAULT;
	} else {
		rounds = (unsigned long) *prefered_rounds;
	}

	/* Sanity checks. The libc should also check this, but this
	 * protects against a rounds_prefix overflow. */
	if (rounds < SHA_ROUNDS_MIN) {
		rounds = SHA_ROUNDS_MIN;
	}

	if (rounds > SHA_ROUNDS_MAX) {
		rounds = SHA_ROUNDS_MAX;
	}

	return rounds;
}

/*
 * Fill a salt prefix specifying the rounds number for the SHA crypt methods
 * to a buffer.
 */
static /*@observer@*/void SHA_salt_rounds_to_buf (char *buf, unsigned long rounds)
{
	const size_t buf_begin = strlen (buf);

	/* Nothing to do here if SHA_ROUNDS_DEFAULT is used. */
	if (rounds == SHA_ROUNDS_DEFAULT) {
		return;
	}

	/*
	 * Check if the result buffer is long enough.
	 * We are going to write a maximum of 17 bytes,
	 * plus one byte for the terminator.
	 *    rounds=XXXXXXXXX$
	 *    00000000011111111
	 *    12345678901234567
	 */
	assert (GENSALT_SETTING_SIZE > buf_begin + 17);

	(void) snprintf (buf + buf_begin, 18, "rounds=%lu$", rounds);
}
#endif /* USE_SHA_CRYPT */

#ifdef USE_BCRYPT
/* Return the the rounds number for the BCRYPT method. */
static /*@observer@*/unsigned long BCRYPT_get_salt_rounds (/*@null@*/const int *prefered_rounds)
{
	unsigned long rounds;

	if (NULL == prefered_rounds) {
		long min_rounds = getdef_long ("BCRYPT_MIN_ROUNDS", -1);
		long max_rounds = getdef_long ("BCRYPT_MAX_ROUNDS", -1);

		if ((-1 == min_rounds) && (-1 == max_rounds)) {
			rounds = B_ROUNDS_DEFAULT;
		} else {
			if (-1 == min_rounds) {
				min_rounds = max_rounds;
			}

			if (-1 == max_rounds) {
				max_rounds = min_rounds;
			}

			if (min_rounds > max_rounds) {
				max_rounds = min_rounds;
			}

			rounds = csrand_interval (min_rounds, max_rounds);
		}
	} else if (0 == *prefered_rounds) {
		rounds = B_ROUNDS_DEFAULT;
	} else {
		rounds = (unsigned long) *prefered_rounds;
	}

	/* Sanity checks. */
	if (rounds < B_ROUNDS_MIN) {
		rounds = B_ROUNDS_MIN;
	}

#if USE_XCRYPT_GENSALT
	if (rounds > B_ROUNDS_MAX) {
		rounds = B_ROUNDS_MAX;
	}
#else /* USE_XCRYPT_GENSALT */
	/*
	 * Use 19 as an upper bound for now,
	 * because musl doesn't allow rounds >= 20.
	 * If musl ever supports > 20 rounds,
	 * rounds should be set to B_ROUNDS_MAX.
	 */
	if (rounds > 19) {
		rounds = 19;
	}
#endif /* USE_XCRYPT_GENSALT */

	return rounds;
}

/*
 * Fill a salt prefix specifying the rounds number for the BCRYPT method
 * to a buffer.
 */
static /*@observer@*/void BCRYPT_salt_rounds_to_buf (char *buf, unsigned long rounds)
{
	const size_t buf_begin = strlen (buf);

	/*
	 * Check if the result buffer is long enough.
	 * We are going to write three bytes,
	 * plus one byte for the terminator.
	 *    XX$
	 *    000
	 *    123
	 */
	assert (GENSALT_SETTING_SIZE > buf_begin + 3);

	(void) snprintf (buf + buf_begin, 4, "%2.2lu$", rounds);
}
#endif /* USE_BCRYPT */

#ifdef USE_YESCRYPT
/* Return the the cost number for the YESCRYPT method. */
static /*@observer@*/unsigned long YESCRYPT_get_salt_cost (/*@null@*/const int *prefered_cost)
{
	unsigned long cost;

	if (NULL == prefered_cost) {
		cost = getdef_num ("YESCRYPT_COST_FACTOR", Y_COST_DEFAULT);
	} else if (0 == *prefered_cost) {
		cost = Y_COST_DEFAULT;
	} else {
		cost = (unsigned long) *prefered_cost;
	}

	/* Sanity checks. */
	if (cost < Y_COST_MIN) {
		cost = Y_COST_MIN;
	}

	if (cost > Y_COST_MAX) {
		cost = Y_COST_MAX;
	}

	return cost;
}

/*
 * Fill a salt prefix specifying the cost for the YESCRYPT method
 * to a buffer.
 */
static /*@observer@*/void YESCRYPT_salt_cost_to_buf (char *buf, unsigned long cost)
{
	const size_t buf_begin = strlen (buf);

	/*
	 * Check if the result buffer is long enough.
	 * We are going to write four bytes,
	 * plus one byte for the terminator.
	 *    jXX$
	 *    0000
	 *    1234
	 */
	assert (GENSALT_SETTING_SIZE > buf_begin + 4);

	buf[buf_begin + 0] = 'j';
	if (cost < 3) {
		buf[buf_begin + 1] = 0x36 + cost;
	} else if (cost < 6) {
		buf[buf_begin + 1] = 0x34 + cost;
	} else {
		buf[buf_begin + 1] = 0x3b + cost;
	}
	buf[buf_begin + 2] = cost >= 3 ? 'T' : '5';
	buf[buf_begin + 3] = '$';
	buf[buf_begin + 4] = '\0';
}
#endif /* USE_YESCRYPT */

#if !USE_XCRYPT_GENSALT
static /*@observer@*/const char *gensalt (size_t salt_size)
{
	static char salt[MAX_SALT_SIZE + 6];

	bzero(salt, MAX_SALT_SIZE + 6);

	assert (salt_size >= MIN_SALT_SIZE &&
	        salt_size <= MAX_SALT_SIZE);
	strcat (salt, l64a (csrand ()));
	do {
		strcat (salt, l64a (csrand ()));
	} while (strlen (salt) < salt_size);

	salt[salt_size] = '\0';

	return salt;
}
#endif /* !USE_XCRYPT_GENSALT */

/*
 * Generate 8 base64 ASCII characters of random salt.  If MD5_CRYPT_ENAB
 * in /etc/login.defs is "yes", the salt string will be prefixed by "$1$"
 * (magic) and pw_encrypt() will execute the MD5-based FreeBSD-compatible
 * version of crypt() instead of the standard one.
 * Other methods can be set with ENCRYPT_METHOD
 *
 * The method can be forced with the meth parameter.
 * If NULL, the method will be defined according to the ENCRYPT_METHOD
 * variable, and if not set according to the MD5_CRYPT_ENAB variable,
 * which can both be set inside the login.defs file.
 *
 * If meth is specified, an additional parameter can be provided.
 *  * For the SHA256 and SHA512 method, this specifies the number of rounds
 *    (if not NULL).
 *  * For the YESCRYPT method, this specifies the cost factor (if not NULL).
 */
/*@observer@*/const char *crypt_make_salt (/*@null@*//*@observer@*/const char *meth, /*@null@*/void *arg)
{
	static char result[GENSALT_SETTING_SIZE];
	size_t salt_len = MAX_SALT_SIZE;
	const char *method;
	unsigned long rounds = 0;

	bzero(result, GENSALT_SETTING_SIZE);

	if (NULL != meth)
		method = meth;
	else {
		method = getdef_str ("ENCRYPT_METHOD");
		if (NULL == method) {
			method = getdef_bool ("MD5_CRYPT_ENAB") ? "MD5" : "DES";
		}
	}

	if (0 == strcmp (method, "MD5")) {
		MAGNUM(result, '1');
		salt_len = MD5_CRYPT_SALT_SIZE;
		rounds = 0;
#ifdef USE_BCRYPT
	} else if (0 == strcmp (method, "BCRYPT")) {
		BCRYPTMAGNUM(result);
		salt_len = BCRYPT_SALT_SIZE;
		rounds = BCRYPT_get_salt_rounds (arg);
		BCRYPT_salt_rounds_to_buf (result, rounds);
#endif /* USE_BCRYPT */
#ifdef USE_YESCRYPT
	} else if (0 == strcmp (method, "YESCRYPT")) {
		MAGNUM(result, 'y');
		salt_len = YESCRYPT_SALT_SIZE;
		rounds = YESCRYPT_get_salt_cost (arg);
		YESCRYPT_salt_cost_to_buf (result, rounds);
#endif /* USE_YESCRYPT */
#ifdef USE_SHA_CRYPT
	} else if (0 == strcmp (method, "SHA256")) {
		MAGNUM(result, '5');
		salt_len = SHA_CRYPT_SALT_SIZE;
		rounds = SHA_get_salt_rounds (arg);
		SHA_salt_rounds_to_buf (result, rounds);
	} else if (0 == strcmp (method, "SHA512")) {
		MAGNUM(result, '6');
		salt_len = SHA_CRYPT_SALT_SIZE;
		rounds = SHA_get_salt_rounds (arg);
		SHA_salt_rounds_to_buf (result, rounds);
#endif /* USE_SHA_CRYPT */
	} else if (0 != strcmp (method, "DES")) {
		fprintf (log_get_logfd(),
			 _("Invalid ENCRYPT_METHOD value: '%s'.\n"
			   "Defaulting to DES.\n"),
			 method);
		salt_len = MAX_SALT_SIZE;
		rounds = 0;
		bzero(result, GENSALT_SETTING_SIZE);
	}

#if USE_XCRYPT_GENSALT
	/*
	 * Prepare DES setting for crypt_gensalt(), if result
	 * has not been filled with anything previously.
	 */
	if ('\0' == result[0]) {
		/* Avoid -Wunused-but-set-variable. */
		salt_len = GENSALT_SETTING_SIZE - 1;
		rounds = 0;
		memset(result, '.', salt_len);
		result[salt_len] = '\0';
	}

	char *retval = crypt_gensalt (result, rounds, NULL, 0);

	/* Should not happen, but... */
	if (NULL == retval) {
		fprintf (log_get_logfd(),
			 _("Unable to generate a salt from setting "
			   "\"%s\", check your settings in "
			   "ENCRYPT_METHOD and the corresponding "
			   "configuration for your selected hash "
			   "method.\n"), result);

		exit (1);
	}

	return retval;
#else /* USE_XCRYPT_GENSALT */
	/* Check if the result buffer is long enough. */
	assert (GENSALT_SETTING_SIZE > strlen (result) + salt_len);

	/* Concatenate a pseudo random salt. */
	strncat (result, gensalt (salt_len),
		 GENSALT_SETTING_SIZE - strlen (result) - 1);

	return result;
#endif /* USE_XCRYPT_GENSALT */
}
