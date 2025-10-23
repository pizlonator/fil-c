/*
 * This function implements the "bigcrypt" algorithm specifically for
 * Linux-PAM.
 *
 * This algorithm is algorithm 0 (default) shipped with the C2 secure
 * implementation of Digital UNIX.
 *
 * Disclaimer: This work is not based on the source code to Digital
 * UNIX, nor am I connected to Digital Equipment Corp, in any way
 * other than as a customer. This code is based on published
 * interfaces and reasonable guesswork.
 *
 * Description: The cleartext is divided into blocks of SEGMENT_SIZE=8
 * characters or less. Each block is encrypted using the standard UNIX
 * libc crypt function. The result of the encryption for one block
 * provides the salt for the succeeding block.
 *
 * Restrictions: The buffer used to hold the encrypted result is
 * statically allocated. (see MAX_PASS_LEN below).  This is necessary,
 * as the returned pointer points to "static data that are overwritten
 * by each call", (XPG3: XSI System Interface + Headers pg 109), and
 * this is a drop in replacement for crypt();
 *
 * Andy Phillips <atp@mssl.ucl.ac.uk>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <security/_pam_macros.h>
#include "pam_inline.h"
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "bigcrypt.h"

/*
 * Max cleartext password length in segments of 8 characters this
 * function can deal with (16 segments of 8 chars= max 128 character
 * password).
 */

#define MAX_PASS_LEN       16
#define SEGMENT_SIZE       8
#define SALT_SIZE          2
#define KEYBUF_SIZE        ((MAX_PASS_LEN*SEGMENT_SIZE)+SALT_SIZE)
#define ESEGMENT_SIZE      11
#define CBUF_SIZE          ((MAX_PASS_LEN*ESEGMENT_SIZE)+SALT_SIZE+1)

char *bigcrypt(const char *key, const char *salt)
{
	char *dec_c2_cryptbuf;
#ifdef HAVE_CRYPT_R
	struct crypt_data *cdata;
#endif
	size_t keylen, n_seg, j;
	char *cipher_ptr, *plaintext_ptr, *tmp_ptr, *salt_ptr;
	char keybuf[KEYBUF_SIZE + 1] = {};

	D(("called with key='%s', salt='%s'.", key, salt));

	/* reset arrays */
	dec_c2_cryptbuf = calloc(1, CBUF_SIZE);
	if (!dec_c2_cryptbuf) {
		return NULL;
	}
#ifdef HAVE_CRYPT_R
	cdata = calloc(1, sizeof(*cdata));
	if(!cdata) {
		free(dec_c2_cryptbuf);
		return NULL;
	}
#endif

	/* fill KEYBUF_SIZE with key */
	strncpy(keybuf, key, KEYBUF_SIZE);

	/* deal with case that we are doing a password check for a
	   conventially encrypted password: the salt will be
	   SALT_SIZE+ESEGMENT_SIZE long. */
	if (strlen(salt) == (SALT_SIZE + ESEGMENT_SIZE))
		keybuf[SEGMENT_SIZE] = '\0';	/* terminate password early(?) */

	keylen = strlen(keybuf);

	if (!keylen) {
		n_seg = 1;
	} else {
		/* work out how many segments */
		n_seg = 1 + ((keylen - 1) / SEGMENT_SIZE);
	}

	if (n_seg > MAX_PASS_LEN)
		n_seg = MAX_PASS_LEN;	/* truncate at max length */

	/* set up some pointers */
	cipher_ptr = dec_c2_cryptbuf;
	plaintext_ptr = keybuf;

	/* do the first block with supplied salt */
#ifdef HAVE_CRYPT_R
	tmp_ptr = crypt_r(plaintext_ptr, salt, cdata);	/* libc crypt_r() */
#else
	tmp_ptr = crypt(plaintext_ptr, salt);	/* libc crypt() */
#endif
	if (tmp_ptr == NULL) {
		pam_overwrite_array(keybuf);
		free(dec_c2_cryptbuf);
#ifdef HAVE_CRYPT_R
		pam_overwrite_object(cdata);
		free(cdata);
#endif
		return NULL;
	}
	/* and place in the static area */
	strncpy(cipher_ptr, tmp_ptr, 13);
#ifndef HAVE_CRYPT_R
	pam_overwrite_string(tmp_ptr);
#endif
	cipher_ptr += ESEGMENT_SIZE + SALT_SIZE;
	plaintext_ptr += SEGMENT_SIZE;	/* first block of SEGMENT_SIZE */

	/* change the salt (1st 2 chars of previous block) - this was found
	   by dowsing */

	salt_ptr = cipher_ptr - ESEGMENT_SIZE;

	/* so far this is identical to "return crypt(key, salt);", if
	   there is more than one block encrypt them... */

	if (n_seg > 1) {
		for (j = 2; j <= n_seg; j++) {

#ifdef HAVE_CRYPT_R
			tmp_ptr = crypt_r(plaintext_ptr, salt_ptr, cdata);
#else
			tmp_ptr = crypt(plaintext_ptr, salt_ptr);
#endif
			if (tmp_ptr == NULL) {
				pam_overwrite_array(keybuf);
				pam_overwrite_string(dec_c2_cryptbuf);
				free(dec_c2_cryptbuf);
#ifdef HAVE_CRYPT_R
				pam_overwrite_object(cdata);
				free(cdata);
#endif
				return NULL;
			}

			/* skip the salt for seg!=0 */
			strncpy(cipher_ptr, (tmp_ptr + SALT_SIZE), ESEGMENT_SIZE);
#ifndef HAVE_CRYPT_R
			pam_overwrite_string(tmp_ptr);
#endif

			cipher_ptr += ESEGMENT_SIZE;
			plaintext_ptr += SEGMENT_SIZE;
			salt_ptr = cipher_ptr - ESEGMENT_SIZE;
		}
	}
	D(("key=|%s|, salt=|%s|\nbuf=|%s|\n", key, salt, dec_c2_cryptbuf));

	pam_overwrite_array(keybuf);
#ifdef HAVE_CRYPT_R
	pam_overwrite_object(cdata);
	free(cdata);
#endif

	/* this is the <NUL> terminated encrypted password */
	return dec_c2_cryptbuf;
}
