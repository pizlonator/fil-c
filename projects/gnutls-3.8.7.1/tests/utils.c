/*
 * Copyright (C) 2004-2012 Free Software Foundation, Inc.
 *
 * Author: Simon Josefsson
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#include <windows.h> /* for Sleep */
#include <winbase.h>
#endif

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>

#include <sys/types.h>

#include "utils.h"

int debug = 0;
int error_count = 0;
int break_on_error = 0;

/* doc/credentials/dhparams/rfc3526-group-14-2048.pem */
const char *pkcs3 =
	"-----BEGIN DH PARAMETERS-----\n"
	"MIIBCAKCAQEA///////////JD9qiIWjCNMTGYouA3BzRKQJOCIpnzHQCC76mOxOb\n"
	"IlFKCHmONATd75UZs806QxswKwpt8l8UN0/hNW1tUcJF5IW1dmJefsb0TELppjft\n"
	"awv/XLb0Brft7jhr+1qJn6WunyQRfEsf5kkoZlHs5Fs9wgB8uKFjvwWY2kg2HFXT\n"
	"mmkWP6j9JM9fg2VdI9yjrZYcYvNWIIVSu57VKQdwlpZtZww1Tkq8mATxdGwIyhgh\n"
	"fDKQXkYuNs474553LBgOhgObJ4Oi7Aeij7XFXfBvTFLJ3ivL9pVYFxg5lUl86pVq\n"
	"5RXSJhiY+gUQFXKOWoqsqmj//////////wIBAg==\n"
	"-----END DH PARAMETERS-----\n";

/* doc/credentials/dhparams/rfc7919-ffdhe2048.pem */
const char *pkcs3_2048 =
	"-----BEGIN DH PARAMETERS-----\n"
	"MIIBCAKCAQEA//////////+t+FRYortKmq/cViAnPTzx2LnFg84tNpWp4TZBFGQz\n"
	"+8yTnc4kmz75fS/jY2MMddj2gbICrsRhetPfHtXV/WVhJDP1H18GbtCFY2VVPe0a\n"
	"87VXE15/V8k1mE8McODmi3fipona8+/och3xWKE2rec1MKzKT0g6eXq8CrGCsyT7\n"
	"YdEIqUuyyOP7uWrat2DX9GgdT0Kj3jlN9K5W7edjcrsZCwenyO4KbXCeAvzhzffi\n"
	"7MA0BM0oNC9hkXL+nOmFg/+OTxIy7vKBg8P+OxtMb61zO7X8vC7CIAXFjvGDfRaD\n"
	"ssbzSibBsu/6iGtCOGEoXJf//////////wIBAg==\n"
	"-----END DH PARAMETERS-----\n";

/* doc/credentials/dhparams/rfc7919-ffdhe3072.pem */
const char *pkcs3_3072 =
	"-----BEGIN DH PARAMETERS-----\n"
	"MIIBiAKCAYEA//////////+t+FRYortKmq/cViAnPTzx2LnFg84tNpWp4TZBFGQz\n"
	"+8yTnc4kmz75fS/jY2MMddj2gbICrsRhetPfHtXV/WVhJDP1H18GbtCFY2VVPe0a\n"
	"87VXE15/V8k1mE8McODmi3fipona8+/och3xWKE2rec1MKzKT0g6eXq8CrGCsyT7\n"
	"YdEIqUuyyOP7uWrat2DX9GgdT0Kj3jlN9K5W7edjcrsZCwenyO4KbXCeAvzhzffi\n"
	"7MA0BM0oNC9hkXL+nOmFg/+OTxIy7vKBg8P+OxtMb61zO7X8vC7CIAXFjvGDfRaD\n"
	"ssbzSibBsu/6iGtCOGEfz9zeNVs7ZRkDW7w09N75nAI4YbRvydbmyQd62R0mkff3\n"
	"7lmMsPrBhtkcrv4TCYUTknC0EwyTvEN5RPT9RFLi103TZPLiHnH1S/9croKrnJ32\n"
	"nuhtK8UiNjoNq8Uhl5sN6todv5pC1cRITgq80Gv6U93vPBsg7j/VnXwl5B0rZsYu\n"
	"N///////////AgEC\n"
	"-----END DH PARAMETERS-----\n";

void _fail(const char *format, ...)
{
	va_list arg_ptr;

	va_start(arg_ptr, format);
#ifdef HAVE_VASPRINTF
	char *str = NULL;
	vasprintf(&str, format, arg_ptr);

	if (str)
		fputs(str, stderr);
#else
	{
		char str[1024];

		vsnprintf(str, sizeof(str), format, arg_ptr);
		fputs(str, stderr);
	}
#endif
	va_end(arg_ptr);
	error_count++;
	exit(1);
}

void fail_ignore(const char *format, ...)
{
	char str[1024];
	va_list arg_ptr;

	va_start(arg_ptr, format);
	vsnprintf(str, sizeof(str), format, arg_ptr);
	va_end(arg_ptr);
	fputs(str, stderr);
	error_count++;
	exit(77);
}

void sec_sleep(int sec)
{
	int ret;
#ifdef HAVE_NANOSLEEP
	struct timespec ts;

	ts.tv_sec = sec;
	ts.tv_nsec = 0;
	do {
		ret = nanosleep(&ts, NULL);
	} while (ret == -1 && errno == EINTR);
	if (ret == -1)
		abort();
#else
	do {
		ret = sleep(sec);
	} while (ret == -1 && errno == EINTR);
#endif
}

void success(const char *format, ...)
{
	char str[1024];
	va_list arg_ptr;

	va_start(arg_ptr, format);
	vsnprintf(str, sizeof(str), format, arg_ptr);
	va_end(arg_ptr);
	fputs(str, stderr);
}

void escapeprint(const char *str, size_t len)
{
	size_t i;

	printf(" (length %d bytes):\n\t'", (int)len);
	for (i = 0; i < len; i++) {
		if (((str[i] & 0xFF) >= 'A' && (str[i] & 0xFF) <= 'Z') ||
		    ((str[i] & 0xFF) >= 'a' && (str[i] & 0xFF) <= 'z') ||
		    ((str[i] & 0xFF) >= '0' && (str[i] & 0xFF) <= '9') ||
		    (str[i] & 0xFF) == ' ' || (str[i] & 0xFF) == '.')
			printf("%c", (str[i] & 0xFF));
		else
			printf("\\x%02X", (str[i] & 0xFF));
		if ((i + 1) % 16 == 0 && (i + 1) < len)
			printf("'\n\t'");
	}
	printf("\n");
}

void c_print(const unsigned char *str, size_t len)
{
	size_t i;

	printf(" (length %d bytes):\n\t\"", (int)len);
	for (i = 0; i < len; i++) {
		printf("\\x%02X", (str[i] & 0xFF));
		if ((i + 1) % 16 == 0 && (i + 1) < len)
			printf("\"\n\t\"");
	}
	printf("\"\n");
}

void hexprint(const void *_str, size_t len)
{
	size_t i;
	const char *str = _str;

	printf("\t;; ");
	for (i = 0; i < len; i++) {
		printf("%02x ", (str[i] & 0xFF));
		if ((i + 1) % 8 == 0)
			printf(" ");
		if ((i + 1) % 16 == 0 && i + 1 < len)
			printf("\n\t;; ");
	}
	printf("\n");
}

void binprint(const void *_str, size_t len)
{
	size_t i;
	const char *str = _str;

	printf("\t;; ");
	for (i = 0; i < len; i++) {
		printf("%d%d%d%d%d%d%d%d ", (str[i] & 0xFF) & 0x80 ? 1 : 0,
		       (str[i] & 0xFF) & 0x40 ? 1 : 0,
		       (str[i] & 0xFF) & 0x20 ? 1 : 0,
		       (str[i] & 0xFF) & 0x10 ? 1 : 0,
		       (str[i] & 0xFF) & 0x08 ? 1 : 0,
		       (str[i] & 0xFF) & 0x04 ? 1 : 0,
		       (str[i] & 0xFF) & 0x02 ? 1 : 0,
		       (str[i] & 0xFF) & 0x01 ? 1 : 0);
		if ((i + 1) % 3 == 0)
			printf(" ");
		if ((i + 1) % 6 == 0 && i + 1 < len)
			printf("\n\t;; ");
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	do
		if (strcmp(argv[argc - 1], "-v") == 0 ||
		    strcmp(argv[argc - 1], "--verbose") == 0)
			debug = 1;
		else if (strcmp(argv[argc - 1], "-b") == 0 ||
			 strcmp(argv[argc - 1], "--break-on-error") == 0)
			break_on_error = 1;
		else if (strcmp(argv[argc - 1], "-h") == 0 ||
			 strcmp(argv[argc - 1], "-?") == 0 ||
			 strcmp(argv[argc - 1], "--help") == 0) {
			printf("Usage: %s [-vbh?] [--verbose] [--break-on-error] [--help]\n",
			       argv[0]);
			return 1;
		}
	while (argc-- > 1);

	doit();

	if (debug || error_count > 0)
		printf("Self test `%s' finished with %d errors\n", argv[0],
		       error_count);

	return error_count ? 1 : 0;
}

struct tmp_file_st {
	char file[TMPNAME_SIZE];
	struct tmp_file_st *next;
};

static struct tmp_file_st *temp_files = (void *)-1;

static void append(const char *file)
{
	struct tmp_file_st *p;

	if (temp_files == (void *)-1)
		return;

	p = calloc(1, sizeof(*p));

	assert(p != NULL);
	snprintf(p->file, sizeof(p->file), "%s", file);
	p->next = temp_files;
	temp_files = p;
}

char *get_tmpname(char s[TMPNAME_SIZE])
{
	unsigned char rnd[6];
	static char _s[TMPNAME_SIZE];
	int ret;
	char *p;
	const char *path;

	ret = gnutls_rnd(GNUTLS_RND_NONCE, rnd, sizeof(rnd));
	if (ret < 0)
		return NULL;

	path = getenv("builddir");
	if (path == NULL)
		path = ".";

	if (s == NULL)
		p = _s;
	else
		p = s;

	snprintf(p, TMPNAME_SIZE, "%s/tmpfile-%02x%02x%02x%02x%02x%02x.tmp",
		 path, (unsigned)rnd[0], (unsigned)rnd[1], (unsigned)rnd[2],
		 (unsigned)rnd[3], (unsigned)rnd[4], (unsigned)rnd[5]);

	append(p);

	return p;
}

void track_temp_files(void)
{
	temp_files = NULL;
}

void delete_temp_files(void)
{
	struct tmp_file_st *p = temp_files;
	struct tmp_file_st *next;

	if (p == (void *)-1)
		return;

	while (p != NULL) {
		remove(p->file);
		next = p->next;
		free(p);
		p = next;
	}
}

#ifndef _WIN32
int tcp_connect(const char *addr, unsigned port)
{
	int sock;
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		return -1;
	}
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
		close(sock);
		return -1;
	}
	if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		close(sock);
		return -1;
	}
	return sock;
}
#endif
