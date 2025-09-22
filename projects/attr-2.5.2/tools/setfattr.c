/*
  File: setfattr.c
  (Linux Extended Attributes)

  Copyright (C) 2001-2002 Andreas Gruenbacher <andreas.gruenbacher@gmail.com>
  Copyright (C) 2001-2002 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <locale.h>
#include <ctype.h>
#include <libgen.h>
#include <sys/xattr.h>
#include <errno.h>

#include "misc.h"

#define CMD_LINE_OPTIONS "n:x:v:h"
#define CMD_LINE_SPEC1 "{-n name} [-v value] [-h] file..."
#define CMD_LINE_SPEC2 "{-x name} [-h] file..."

static const struct option long_options[] = {
	{ "name",		1, 0, 'n' }, 
	{ "remove",		1, 0, 'x' },
	{ "value",		1, 0, 'v' },
	{ "no-dereference",	0, 0, 'h' },
	{ "restore",		1, 0, 'B' },
	{ "raw",		0, 0, CHAR_MAX + 1 },
	{ "version",		0, 0, 'V' },
	{ "help",		0, 0, 'H' },
	{ NULL,			0, 0, 0 }
};

char *opt_name;  /* attribute name to set */
char *opt_value;  /* attribute value */
int opt_set;  /* set an attribute */
int opt_remove;  /* remove an attribute */
int opt_restore;  /* restore has been run */
int opt_deref = 1;  /* dereference symbolic links */
int opt_raw;  /* attribute value is not encoded */

int had_errors;
const char *progname;

int do_set(const char *path, const char *name, const char *value);
const char *decode(const char *value, size_t *size);
int restore(const char *filename);
int hex_digit(char c);
int base64_digit(char c);

const char *strerror_ea(int err)
{
#ifdef __linux__
	/* The Linux kernel does not define ENOATTR, but maps it to ENODATA. */
	if (err == ENODATA)
		return _("No such attribute");
#endif
	return strerror(err);
}

static const char *xquote(const char *str, const char *quote_chars)
{
	const char *q = quote(str, quote_chars);
	if (q == NULL) {
		fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		exit(1);
	}
	return q;
}

int do_setxattr(const char *path, const char *name,
		const void *value, size_t size)
{
	return (opt_deref ? setxattr : lsetxattr)(path, name, value, size, 0);
}

int do_removexattr(const char *path, const char *name)
{
	return (opt_deref ? removexattr : lremovexattr)(path, name);
}

int restore(const char *filename)
{
	static char *path;
	static size_t path_size;
	FILE *file;
	char *l;
	int line = 0, backup_line, status = 0;
	
	if (strcmp(filename, "-") == 0)
		file = stdin;
	else {
		file = fopen(filename, "r");
		if (file == NULL) {
				fprintf(stderr, "%s: %s: %s\n",
					progname, filename, strerror_ea(errno));
				return 1;
		}
	}

	for(;;) {
		backup_line = line;
		while ((l = next_line(file)) != NULL && *l == '\0')
			line++;
		if (l == NULL)
			break;
		line++;
		if (strncmp(l, "# file: ", 8) != 0) {
			if (file != stdin) {
				fprintf(stderr, _("%s: %s: No filename found "
				                  "in line %d, aborting\n"),
					progname, filename, backup_line);
			} else {
				fprintf(stderr, _("%s: No filename found in "
			                          "line %d of standard input, "
						  "aborting\n"),
					  progname, backup_line);
			}
			status = 1;
			goto cleanup;
		} else
			l += 8;
		l = unquote(l);
		if (high_water_alloc((void **)&path, &path_size, strlen(l)+1)) {
			perror(progname);
			status = 1;
			goto cleanup;
		}
		strcpy(path, l);

		while ((l = next_line(file)) != NULL && *l != '\0') {
			char *name = l, *value = strchr(l, '=');
			line++;
			if (value)
				*value++ = '\0';
			status = do_set(path, unquote(name), value);
		}
		if (l == NULL)
			break;
		line++;
	}
	if (!feof(file)) {
		fprintf(stderr, "%s: %s: %s\n", progname, filename,
			strerror(errno));
		if (!status)
			status = 1;
	}

cleanup:
	if (path)
		free(path);
	if (file != stdin)
		fclose(file);
	if (status)
		had_errors++;
	return status;
}

void help(void)
{
	printf(_("%s %s -- set extended attributes\n"), progname, VERSION);
	printf(_("Usage: %s %s\n"), progname, CMD_LINE_SPEC1);
	printf(_("       %s %s\n"), progname, CMD_LINE_SPEC2);
	printf(_(
"  -n, --name=name         set the value of the named extended attribute\n"
"  -x, --remove=name       remove the named extended attribute\n"
"  -v, --value=value       use value as the attribute value\n"
"  -h, --no-dereference    do not dereference symbolic links\n"
"      --restore=file      restore extended attributes\n"
"      --raw               attribute value is not encoded\n"
"      --version           print version and exit\n"
"      --help              this help text\n"));
}

int main(int argc, char *argv[])
{
	int opt;

	progname = basename(argv[0]);

	setlocale(LC_CTYPE, "");
	setlocale(LC_MESSAGES, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((opt = getopt_long(argc, argv, CMD_LINE_OPTIONS,
		                  long_options, NULL)) != -1) {
		switch(opt) {
			case 'n':  /* attribute name */
				if (opt_name || opt_remove)
					goto synopsis;
				opt_name = optarg;
				opt_set = 1;
				break;

			case 'h':  /* set attribute on symlink itself */
				opt_deref = 0;
				break;

			case 'v':  /* attribute value */
				if (opt_value || opt_remove)
					goto synopsis;
				opt_value = optarg;
				break;

			case CHAR_MAX + 1:
				opt_raw = 1;
				break;

			case 'x':  /* remove attribute */
				if (opt_name || opt_set)
					goto synopsis;
				opt_name = optarg;
				opt_remove = 1;
				break;

			case 'B':  /* restore */
				opt_restore = 1;
				restore(optarg);
				break;

			case 'V':
				printf("%s " VERSION "\n", progname);
				return 0;

			case 'H':
				help();
				return 0;

			default:
				goto synopsis;
		}
	}
	if (!(((opt_remove || opt_set) && optind < argc) || opt_restore))
		goto synopsis;

	while (optind < argc) {
		do_set(argv[optind], unquote(opt_name), opt_value);
		optind++;
	}

	return (had_errors ? 1 : 0);

synopsis:
	fprintf(stderr, _("Usage: %s %s\n"
			  "       %s %s\n"
	                  "Try `%s --help' for more information.\n"),
		progname, CMD_LINE_SPEC1, progname, CMD_LINE_SPEC2, progname);
	return 2;
}

int do_set(const char *path, const char *name, const char *value)
{
	size_t size = 0;
	int error;

	if (value) {
		size = strlen(value);
		if (!opt_raw)
			value = decode(value, &size);
		if (!value)
			return 1;
	}
	if (opt_set || opt_restore)
		error = do_setxattr(path, name, value, size);
	else
		error = do_removexattr(path, name);

	if (error < 0) {
		fprintf(stderr, "%s: %s: %s\n",
			progname, xquote(path, "\n\r"), strerror_ea(errno));
		had_errors++;
		return 1;
	}
	return 0;
}

const char *decode(const char *value, size_t *size)
{
	static char *decoded;
	static size_t decoded_size;

	if (*size == 0)
		return "";
	if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
		const char *v = value+2, *end = value + *size;
		char *d;

		if (high_water_alloc((void **)&decoded, &decoded_size,
				     *size / 2)) {
			fprintf(stderr, "%s: %s\n",
				progname, strerror_ea(errno));
			had_errors++;
			return NULL;
		}
		d = decoded;
		while (v < end) {
			int d1, d0;

			while (v < end && isspace(*v))
				v++;
			if (v == end)
				break;
			d1 = hex_digit(*v++);
			while (v < end && isspace(*v))
				v++;
			if (v == end) {
		bad_hex_encoding:
				fprintf(stderr, "bad input encoding\n");
				had_errors++;
				return NULL;
			}
			d0 = hex_digit(*v++);
			if (d1 < 0 || d0 < 0)
				goto bad_hex_encoding;
			*d++ = ((d1 << 4) | d0);
		}
		*size = d - decoded;
	} else if (value[0] == '0' && (value[1] == 's' || value[1] == 'S')) {
		const char *v = value+2, *end = value + *size;
		int d0, d1, d2, d3;
		char *d;

		if (high_water_alloc((void **)&decoded, &decoded_size,
				     *size / 4 * 3)) {
			fprintf(stderr, "%s: %s\n",
				progname, strerror_ea(errno));
			had_errors++;
			return NULL;
		}
		d = decoded;
		for(;;) {
			while (v < end && isspace(*v))
				v++;
			if (v == end) {
				d0 = d1 = d2 = d3 = -2;
				break;
			}
			if (v + 4 > end) {
		bad_base64_encoding:
				fprintf(stderr, "bad input encoding\n");
				had_errors++;
				return NULL;
			}
			d0 = base64_digit(*v++);
			d1 = base64_digit(*v++);
			d2 = base64_digit(*v++);
			d3 = base64_digit(*v++);
			if (d0 < 0 || d1 < 0 || d2 < 0 || d3 < 0)
				break;

			*d++ = (char)((d0 << 2) | (d1 >> 4));
			*d++ = (char)((d1 << 4) | (d2 >> 2));
			*d++ = (char)((d2 << 6) | d3);
		}
		if (d0 == -2) {
			if (d1 != -2 || d2 != -2 || d3 != -2)
				goto bad_base64_encoding;
			goto base64_end;
		}
		if (d0 == -1 || d1 < 0 || d2 == -1 || d3 == -1)
			goto bad_base64_encoding;
		*d++ = (char)((d0 << 2) | (d1 >> 4));
		if (d2 != -2)
			*d++ = (char)((d1 << 4) | (d2 >> 2));
		else {
			if (d1 & 0x0F || d3 != -2)
				goto bad_base64_encoding;
			goto base64_end;
		}
		if (d3 != -2)
			*d++ = (char)((d2 << 6) | d3);
		else if (d2 & 0x03)
			goto bad_base64_encoding;
	base64_end:
		while (v < end && isspace(*v))
			v++;
		if (v + 4 <= end && *v == '=') {
			if (*++v != '=' || *++v != '=' || *++v != '=')
				goto bad_base64_encoding;
			v++;
		}
		while (v < end && isspace(*v))
			v++;
		if (v < end)
			goto bad_base64_encoding;
		*size = d - decoded;
	} else {
		const char *v = value, *end = value + *size;
		char *d;

		if (end > v+1 && *v == '"' && *(end-1) == '"') {
			v++;
			end--;
		}

		if (high_water_alloc((void **)&decoded, &decoded_size, *size)) {
			fprintf(stderr, "%s: %s\n",
				progname, strerror_ea(errno));
			had_errors++;
			return NULL;
		}
		d = decoded;

		while (v < end) {
			if (v[0] == '\\') {
				if (v[1] == '\\' || v[1] == '"') {
					*d++ = *++v; v++;
				} else if (v[1] >= '0' && v[1] <= '7') {
					int c = 0;
					v++;
					c = (*v++ - '0');
					if (*v >= '0' && *v <= '7')
						c = (c << 3) + (*v++ - '0');
					if (*v >= '0' && *v <= '7')
						c = (c << 3) + (*v++ - '0');
					*d++ = c;
				} else
					*d++ = *v++;
			} else
				*d++ = *v++;
		}
		*size = d - decoded;
	}
	return decoded;
}

int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else
		return -1;
}

int base64_digit(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	else if (c >= 'a' && c <= 'z')
		return 26 + c - 'a';
	else if (c >= '0' && c <= '9')
		return 52 + c - '0';
	else if (c == '+')
		return 62;
	else if (c == '/')
		return 63;
	else if (c == '=')
		return -2;
	else
		return -1;
}

