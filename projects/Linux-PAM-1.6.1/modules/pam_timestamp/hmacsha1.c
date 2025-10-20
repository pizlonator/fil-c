/* An implementation of HMAC using SHA-1.
 *
 * Copyright (c) 2003 Red Hat, Inc.
 * Written by Nalin Dahyabhai <nalin@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/* See RFC 2104 for descriptions. */
#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <security/pam_ext.h>
#include "pam_inline.h"
#include "hmacsha1.h"
#include "sha1.h"

#define MINIMUM_KEY_SIZE SHA1_OUTPUT_SIZE
#define MAXIMUM_KEY_SIZE SHA1_BLOCK_SIZE

static void
hmac_key_create(pam_handle_t *pamh, const char *filename, size_t key_size,
		uid_t owner, gid_t group)
{
	int randfd, keyfd, i;
	size_t count;
	char *key;

	/* Open the destination file. */
	keyfd = open(filename,
		     O_WRONLY | O_CREAT | O_EXCL | O_TRUNC,
		     S_IRUSR | S_IWUSR);
	if (keyfd == -1) {
		pam_syslog(pamh, LOG_ERR, "Cannot create %s: %m", filename);
		return;
	}


	 if (fchown(keyfd, owner, group) == -1) {
		pam_syslog(pamh, LOG_ERR, "Cannot chown %s: %m", filename);
		close(keyfd);
		return;
	}

	/* Open the random device to get key data. */
	randfd = open("/dev/urandom", O_RDONLY);
	if (randfd == -1) {
		pam_syslog(pamh, LOG_ERR, "Cannot open /dev/urandom: %m");
		close(keyfd);
		return;
	}

	/* Read random data for use as the key. */
	key = malloc(key_size);
	count = 0;
	if (!key) {
		close(keyfd);
		close(randfd);
		return;
	}
	while (count < key_size) {
		i = read(randfd, key + count, key_size - count);
		if ((i == 0) || (i == -1)) {
			break;
		}
		count += i;
	}

	close(randfd);

	/* If we didn't get enough, stop here. */
	if (count < key_size) {
		pam_syslog(pamh, LOG_ERR, "Short read on random device");
		pam_overwrite_n(key, key_size);
		free(key);
		close(keyfd);
		return;
	}

	/* Now write the key. */
	count = 0;
	while (count < key_size) {
		i = write(keyfd, key + count, key_size - count);
		if ((i == 0) || (i == -1)) {
			break;
		}
		count += i;
	}
	pam_overwrite_n(key, key_size);
	free(key);
	close(keyfd);
}

static void
hmac_key_read(pam_handle_t *pamh, const char *filename, size_t default_key_size,
	      uid_t owner, gid_t group,
	      void **key, size_t *key_size)
{
	char *tmp;
	int keyfd, i, count;
	struct stat st;

	tmp = NULL;
	*key = NULL;
	*key_size = 0;

	/* Try to open the key file. */
	keyfd = open(filename, O_RDONLY);
	if (keyfd == -1) {
		/* No such thing? Create it. */
		if (errno == ENOENT) {
			hmac_key_create(pamh, filename, default_key_size,
					owner, group);
			keyfd = open(filename, O_RDONLY);
		} else {
			pam_syslog(pamh, LOG_ERR, "Cannot open %s: %m", filename);
		}
		if (keyfd == -1)
			return;
	}

	/* If we failed to open the file, we're done. */
	if (fstat(keyfd, &st) == -1) {
		close(keyfd);
		return;
	}

	/* Read the contents of the file. */
	tmp = malloc(st.st_size);
	if (!tmp) {
		close(keyfd);
		return;
	}

	count = 0;
	while (count < st.st_size) {
		i = read(keyfd, tmp + count, st.st_size - count);
		if ((i == 0) || (i == -1)) {
			break;
		}
		count += i;
	}
	close(keyfd);

	/* Require that we got the expected amount of data. */
	if (count < st.st_size) {
		pam_overwrite_n(tmp, st.st_size);
		free(tmp);
		return;
	}

	/* Pass the key back. */
	*key = tmp;
	*key_size = st.st_size;
}

static void
xor_block(unsigned char *p, unsigned char byte, size_t length)
{
	size_t i;
	for (i = 0; i < length; i++) {
		p[i] = p[i] ^ byte;
	}
}

void
hmac_sha1_generate(void **mac, size_t *mac_length,
		   const void *raw_key, size_t raw_key_size,
		   const void *text, size_t text_length)
{
	unsigned char key[MAXIMUM_KEY_SIZE] = {}, tmp_key[MAXIMUM_KEY_SIZE];
	size_t maximum_key_size = SHA1_BLOCK_SIZE,
	       minimum_key_size = SHA1_OUTPUT_SIZE;
	const unsigned char ipad = 0x36, opad = 0x5c;
	struct sha1_context sha1;
	unsigned char inner[SHA1_OUTPUT_SIZE], outer[SHA1_OUTPUT_SIZE];

	*mac = NULL;
	*mac_length = 0;

#ifndef HMAC_ALLOW_SHORT_KEYS
	/* If the key is too short, don't bother. */
	if (raw_key_size < minimum_key_size) {
		return;
	}
#endif

	/* If the key is too long, "compress" it, else copy it and pad it
	 * out with zero bytes. */
	if (raw_key_size > maximum_key_size) {
		sha1_init(&sha1);
		sha1_update(&sha1, raw_key, raw_key_size);
		sha1_output(&sha1, key);
	} else {
		memmove(key, raw_key, raw_key_size);
	}

	/* Generate the inner sum. */
	memcpy(tmp_key, key, sizeof(tmp_key));
	xor_block(tmp_key, ipad, sizeof(tmp_key));

	sha1_init(&sha1);
	sha1_update(&sha1, tmp_key, sizeof(tmp_key));
	sha1_update(&sha1, text, text_length);
	sha1_output(&sha1, inner);

	/* Generate the outer sum. */
	memcpy(tmp_key, key, sizeof(tmp_key));
	xor_block(tmp_key, opad, sizeof(tmp_key));

	sha1_init(&sha1);
	sha1_update(&sha1, tmp_key, sizeof(tmp_key));
	sha1_update(&sha1, inner, sizeof(inner));
	sha1_output(&sha1, outer);

	/* We don't need any of the keys any more. */
	pam_overwrite_array(key);
	pam_overwrite_array(tmp_key);

	/* Allocate space to store the output. */
	*mac_length = sizeof(outer);
	*mac = malloc(*mac_length);
	if (*mac == NULL) {
		*mac_length = 0;
		return;
	}

	memcpy(*mac, outer, *mac_length);
}

void
hmac_sha1_generate_file(pam_handle_t *pamh, void **mac, size_t *mac_length,
			const char *keyfile, uid_t owner, gid_t group,
			const void *text, size_t text_length)
{
	void *key;
	size_t key_length;

	hmac_key_read(pamh, keyfile,
		      MAXIMUM_KEY_SIZE, owner, group,
		      &key, &key_length);
	if (key == NULL) {
		*mac = NULL;
		*mac_length = 0;
		return;
	}
	hmac_sha1_generate(mac, mac_length,
			   key, key_length,
			   text, text_length);
	pam_overwrite_n(key, key_length);
	free(key);
}

size_t
hmac_sha1_size(void)
{
	return SHA1_OUTPUT_SIZE;
}
