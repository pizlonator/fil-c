/*
 * Copyright (C) 2005-2012 Free Software Foundation, Inc.
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

/* Gnulib portability files. */

#ifndef ENABLE_PSK

#include <stdio.h>

int main(int argc, char **argv)
{
	printf("\nPSK not supported. This program is a dummy.\n\n");
	return 1;
};

#else

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gnutls/gnutls.h>
#include "psktool-options.h"

#include <gnutls/crypto.h> /* for random */

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

/* Gnulib portability files. */
#include <minmax.h>
#include "close-stream.h"
#include "getpass.h"
#include "xsize.h"

static int write_key(const char *username, const unsigned char *key,
		     size_t key_size, const char *passwd_file);

#define MAX_KEY_SIZE 512
int main(int argc, char **argv)
{
	int ret;
#ifndef _WIN32
	struct passwd *pwd;
#endif
	unsigned char key[MAX_KEY_SIZE];
	size_t key_size;
	const char *passwd, *username;

	if ((ret = gnutls_global_init()) < 0) {
		fprintf(stderr, "global_init: %s\n", gnutls_strerror(ret));
		exit(1);
	}

	umask(066);

	optionProcess(&psktoolOptions, argc, argv);

	if (!HAVE_OPT(PSKFILE)) {
		fprintf(stderr, "You need to specify a PSK key file\n");
		exit(1);
	} else
		passwd = OPT_ARG(PSKFILE);

	if (!HAVE_OPT(USERNAME)) {
#ifndef _WIN32
		pwd = getpwuid(getuid());

		if (pwd == NULL) {
			fprintf(stderr, "No such user\n");
			return -1;
		}

		username = pwd->pw_name;
#else
		fprintf(stderr, "Please specify a user\n");
		return -1;
#endif
	} else
		username = OPT_ARG(USERNAME);

	if (HAVE_OPT(KEYSIZE) && OPT_VALUE_KEYSIZE > MAX_KEY_SIZE) {
		fprintf(stderr, "Key size is too long\n");
		exit(1);
	}

	if (!HAVE_OPT(KEYSIZE) || OPT_VALUE_KEYSIZE < 1)
		key_size = 32;
	else
		key_size = OPT_VALUE_KEYSIZE;

	printf("Generating a random key for user '%s'\n", username);

	ret = gnutls_rnd(GNUTLS_RND_RANDOM, (char *)key, key_size);
	if (ret < 0) {
		fprintf(stderr, "Not enough randomness\n");
		exit(1);
	}

	ret = write_key(username, key, key_size, passwd);
	if (ret == 0)
		printf("Key stored to %s\n", passwd);

	return ret;
}

static int filecopy(const char *src, const char *dst)
{
	FILE *fp, *fp2;
	char line[5 * 1024];
	char *p;

	fp = fopen(dst, "w");
	if (fp == NULL) {
		fprintf(stderr, "Cannot open '%s' for write\n", dst);
		return -1;
	}

	fp2 = fopen(src, "r");
	if (fp2 == NULL) {
		/* empty file */
		fclose(fp);
		return 0;
	}

	line[sizeof(line) - 1] = 0;
	do {
		p = fgets(line, sizeof(line) - 1, fp2);
		if (p == NULL)
			break;

		fputs(line, fp);
	} while (1);

	fclose(fp);
	fclose(fp2);

	return 0;
}

static int write_key(const char *username, const unsigned char *key,
		     size_t key_size, const char *passwd_file)
{
	FILE *fp;
	char line[5 * 1024];
	char *p, *pp;
	char tmpname[1024];
	gnutls_datum_t tmp, _username = { NULL, 0 }, _key = { NULL, 0 };
	struct stat st;
	FILE *fp2;
	bool put;
	int ret = 0;

	if (strlen(passwd_file) + 5 > sizeof(tmpname)) {
		fprintf(stderr, "file '%s' is tooooo long\n", passwd_file);
		return -1;
	}

	snprintf(tmpname, sizeof(tmpname), "%s.tmp", passwd_file);

	if (stat(tmpname, &st) != -1) {
		fprintf(stderr, "file '%s' is locked\n", tmpname);
		return -1;
	}

	if (filecopy(passwd_file, tmpname) != 0) {
		fprintf(stderr, "Cannot copy '%s' to '%s'\n", passwd_file,
			tmpname);
		return -1;
	}

	fp = fopen(passwd_file, "w");
	if (fp == NULL) {
		fprintf(stderr, "Cannot open '%s' for write\n", passwd_file);
		(void)remove(tmpname);
		return -1;
	}

	fp2 = fopen(tmpname, "r");
	if (fp2 == NULL) {
		fprintf(stderr, "Cannot open '%s' for read\n", tmpname);
		(void)remove(tmpname);
		fclose(fp);
		return -1;
	}

	/* encode username if it contains special characters */
	if (strcspn(username, ":\n") != strlen(username)) {
		char *new_data;
		size_t new_size;

		tmp.data = (void *)username;
		tmp.size = strlen(username);

		ret = gnutls_hex_encode2(&tmp, &_username);
		if (ret < 0) {
			fprintf(stderr, "HEX encoding error\n");
			ret = -1;
			goto out;
		}

		/* prepend '#' */
		new_size = xsum(_username.size, 2);
		if (size_overflow_p(new_size)) {
			ret = -1;
			goto out;
		}
		new_data = gnutls_realloc(_username.data, new_size);
		if (!new_data) {
			ret = -1;
			goto out;
		}
		memmove(new_data + 1, new_data, _username.size);
		new_data[0] = '#';
		new_data[_username.size + 1] = '\0';
		_username.data = (void *)new_data;
		_username.size = new_size - 1;
	} else {
		_username.data = (void *)gnutls_strdup(username);
		if (!_username.data) {
			ret = -1;
			goto out;
		}
		_username.size = strlen(username);
	}

	/* encode key */
	tmp.data = (void *)key;
	tmp.size = key_size;

	ret = gnutls_hex_encode2(&tmp, &_key);
	if (ret < 0) {
		fprintf(stderr, "HEX encoding error\n");
		ret = -1;
		goto out;
	}

	put = false;
	while (true) {
		p = fgets(line, sizeof(line) - 1, fp2);
		if (p == NULL)
			break;

		pp = strchr(line, ':');
		if (pp == NULL)
			continue;

		if (strncmp(p, (const char *)_username.data,
			    MAX(_username.size, (unsigned int)(pp - p))) == 0) {
			put = true;
			fprintf(fp, "%s:%s\n", _username.data, _key.data);
		} else {
			fputs(line, fp);
		}
	}

	if (!put) {
		fprintf(fp, "%s:%s\n", _username.data, _key.data);
	}

out:
	if (close_stream(fp) == EOF) {
		fprintf(stderr, "Error writing %s: %s\n", passwd_file,
			strerror(errno));
		ret = -1;
	}

	fclose(fp2);

	(void)remove(tmpname);
	gnutls_free(_username.data);
	gnutls_free(_key.data);

	return ret;
}

#endif /* ENABLE_PSK */
