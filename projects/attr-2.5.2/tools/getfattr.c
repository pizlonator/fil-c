/*
  File: getfattr.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <regex.h>
#include <locale.h>
#include <libgen.h>
#include <sys/xattr.h>

#include "walk_tree.h"
#include "misc.h"

#define CMD_LINE_OPTIONS "n:de:m:hRLP"
#define CMD_LINE_SPEC "[-hRLP] [-n name|-d] [-e en] [-m pattern] path..."

static const struct option long_options[] = {
	{ "name",		1, 0, 'n' },
	{ "dump",		0, 0, 'd' },
	{ "encoding",		1, 0, 'e' },
	{ "match",		1, 0, 'm' },
	{ "only-values",	0, 0, 'v' },
	{ "no-dereference",	0, 0, 'h' },
	{ "absolute-names",	0, 0, 'a' },
	{ "one-file-system",	0, 0, 1 },
	{ "recursive",		0, 0, 'R' },
	{ "logical",		0, 0, 'L' },
	{ "physical",		0, 0, 'P' },
	{ "version",		0, 0, 'V' },
	{ "help",		0, 0, 'H' },
	{ NULL,			0, 0, 0 }
};

int walk_flags = WALK_TREE_DEREFERENCE;
int opt_dump;  /* dump attribute values (or only list the names) */
char *opt_name;  /* dump named attributes */
char *opt_name_pattern = "^user\\.";  /* include only matching names */
char *opt_encoding;  /* encode values automatically (NULL), or as "text",
                        "hex", or "base64" */
char opt_value_only;  /* dump the value only, without any decoration */
int opt_strip_leading_slash = 1;  /* strip leading '/' from path names */

const char *progname;
int absolute_warning;
int had_errors;
regex_t name_regex;


static const char *xquote(const char *str, const char *quote_chars)
{
	const char *q = quote(str, quote_chars);
	if (q == NULL) {
		fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		exit(1);
	}
	return q;
}

int do_getxattr(const char *path, const char *name, void *value, size_t size)
{
	return ((walk_flags & WALK_TREE_DEREFERENCE) ?
		getxattr : lgetxattr)(path, name, value, size);
}

int do_listxattr(const char *path, char *list, size_t size)
{
	return ((walk_flags & WALK_TREE_DEREFERENCE) ?
		listxattr : llistxattr)(path, list, size);
}

const char *strerror_ea(int err)
{
#ifdef __linux__
	/* The Linux kernel does not define ENOATTR, but maps it to ENODATA. */
	if (err == ENODATA)
		return _("No such attribute");
#endif
	return strerror(err);
}

int pstrcmp(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
}

int well_enough_printable(const char *value, size_t size)
{
	size_t n, nonpr = 0;

	/* Don't count the NULL terminator if there is one */
	if (size && !value[size - 1])
		size--;

	for (n=0; n < size; n++)
		if (!isprint(*value++))
			nonpr++;

	return (size >= nonpr*8);  /* no more than 1/8 non-printable chars */
}

const char *encode(const char *value, size_t *size)
{
	static char *encoded;
	static size_t encoded_size;
	char *enc, *e;
	
	if (opt_encoding == NULL) {
		if (well_enough_printable(value, *size))
			enc = "text";
		else
			enc = "base64";
	} else
		enc = opt_encoding;

	if (strcmp(enc, "text") == 0) {
		size_t n, extra = 0;

		for (e=(char *)value; e < value + *size; e++) {
			if (*e == '\0' || *e == '\n' || *e == '\r')
				extra += 4;
			else if (*e == '\\' || *e == '"')
				extra++;
		}
		if (high_water_alloc((void **)&encoded, &encoded_size,
				     *size + extra + 3)) {
			perror(progname);
			had_errors++;
			return NULL;
		}
		e = encoded;
		*e++='"';
		for (n = 0; n < *size; n++, value++) {
			if (*value == '\0' && n + 1 == *size)
				break;
			if (*value == '\0' || *value == '\n' || *value == '\r') {
				*e++ = '\\';
				*e++ = '0' + ((unsigned char)*value >> 6);
				*e++ = '0' + (((unsigned char)*value & 070) >> 3);
				*e++ = '0' + ((unsigned char)*value & 07);
			} else if (*value == '\\' || *value == '"') {
				*e++ = '\\';
				*e++ = *value;
			} else {
				*e++ = *value;
			}
		}
		*e++ = '"';
		*e = '\0';
		*size = (e - encoded);
	} else if (strcmp(enc, "hex") == 0) {
		static const char *digits = "0123456789abcdef";
		size_t n;

		if (high_water_alloc((void **)&encoded, &encoded_size,
				     *size * 2 + 4)) {
			perror(progname);
			had_errors++;
			return NULL;
		}
		e = encoded;
		*e++='0'; *e++ = 'x';
		for (n = 0; n < *size; n++, value++) {
			*e++ = digits[((unsigned char)*value >> 4)];
			*e++ = digits[((unsigned char)*value & 0x0F)];
		}
		*e = '\0';
		*size = (e - encoded);
	} else if (strcmp(enc, "base64") == 0) {
		static const char *digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
					    "ghijklmnopqrstuvwxyz0123456789+/";
		size_t n;

		if (high_water_alloc((void **)&encoded, &encoded_size,
				     (*size + 2) / 3 * 4 + 1)) {
			perror(progname);
			had_errors++;
			return NULL;
		}
		e = encoded;
		*e++='0'; *e++ = 's';
		for (n=0; n + 2 < *size; n += 3) {
			*e++ = digits[(unsigned char)value[0] >> 2];
			*e++ = digits[(((unsigned char)value[0] & 0x03) << 4) |
			              (((unsigned char)value[1] & 0xF0) >> 4)];
			*e++ = digits[(((unsigned char)value[1] & 0x0F) << 2) |
			              ((unsigned char)value[2] >> 6)];
			*e++ = digits[(unsigned char)value[2] & 0x3F];
			value += 3;
		}
		if (*size - n == 2) {
			*e++ = digits[(unsigned char)value[0] >> 2];
			*e++ = digits[(((unsigned char)value[0] & 0x03) << 4) |
			              (((unsigned char)value[1] & 0xF0) >> 4)];
			*e++ = digits[((unsigned char)value[1] & 0x0F) << 2];
			*e++ = '=';
		} else if (*size - n == 1) {
			*e++ = digits[(unsigned char)value[0] >> 2];
			*e++ = digits[((unsigned char)value[0] & 0x03) << 4];
			*e++ = '=';
			*e++ = '=';
		}
		*e = '\0';
		*size = (e - encoded);
	}
	return encoded;
}

int print_attribute(const char *path, const char *name, int *header_printed)
{
	static char *value;
	static size_t value_size;
	int rval = 0;
	size_t length = 0;

	if (opt_dump || opt_value_only) {
		rval = do_getxattr(path, name, NULL, 0);
		if (rval < 0) {
			fprintf(stderr, "%s: ", xquote(path, "\n\r"));
			fprintf(stderr, "%s: %s\n", xquote(name, "\n\r"),
				strerror_ea(errno));
			return 1;
		}
		if (high_water_alloc((void **)&value, &value_size, rval)) {
			perror(progname);
			had_errors++;
			return 1;
		}
		rval = do_getxattr(path, name, value, value_size);
		if (rval < 0) {
			fprintf(stderr, "%s: ", xquote(path, "\n\r"));
			fprintf(stderr, "%s: %s\n", xquote(name, "\n\r"),
				strerror_ea(errno));
			return 1;
		}
		length = rval;
	}

	if (opt_strip_leading_slash) {
		if (*path == '/') {
			if (!absolute_warning) {
				fprintf(stderr, _("%s: Removing leading '/' "
					"from absolute path names\n"),
					progname);
				absolute_warning = 1;
			}
			while (*path == '/')
				path++;
		} else if (*path == '.' && *(path+1) == '/')
			while (*++path == '/')
				/* nothing */ ;
		if (*path == '\0')
			path = ".";
	}

	if (!*header_printed && !opt_value_only) {
		printf("# file: %s\n", xquote(path, "\n\r"));
		*header_printed = 1;
	}

	if (opt_value_only)
		fwrite(value, length, 1, stdout);
	else if (opt_dump) {
		const char *enc = encode(value, &length);
		
		if (enc)
			printf("%s=%s\n", xquote(name, "=\n\r"), enc);
	} else
		puts(xquote(name, "=\n\r"));

	return 0;
}

int list_attributes(const char *path, int *header_printed)
{
	static char *list;
	static size_t list_size;
	static char **names;
	static size_t names_size;
	int num_names = 0;
	ssize_t length;
	char *l;

	length = do_listxattr(path, NULL, 0);
	if (length < 0) {
		fprintf(stderr, "%s: %s: %s\n", progname, xquote(path, "\n\r"),
			strerror_ea(errno));
		had_errors++;
		return 1;
	} else if (length == 0)
		return 0;
		
	if (high_water_alloc((void **)&list, &list_size, length)) {
		perror(progname);
		had_errors++;
		return 1;
	}

	length = do_listxattr(path, list, list_size);
	if (length < 0) {
		perror(xquote(path, "\n\r"));
		had_errors++;
		return 1;
	}

	for (l = list; l != list + length; l = strchr(l, '\0')+1) {
		if (*l == '\0')	/* not a name, kernel bug */
			continue;

		if (regexec(&name_regex, l, 0, NULL, 0) != 0)
			continue;

		if (names_size < (num_names+1) * sizeof(*names)) {
			if (high_water_alloc((void **)&names, &names_size,
				             (num_names+1) * sizeof(*names))) {
				perror(progname);
				had_errors++;
				return 1;
			}
		}

		names[num_names++] = l;
	}

	qsort(names, num_names, sizeof(*names), pstrcmp);

	if (num_names) {
		int n;

		for (n = 0; n < num_names; n++)
			print_attribute(path, names[n], header_printed);
	}
	return 0;
}

int do_print(const char *path, const struct stat *stat, int walk_flags,
	     void *unused)
{
	int header_printed = 0;
	int err = 0;

	if (walk_flags & WALK_TREE_FAILED) {
		fprintf(stderr, "%s: %s: %s\n", progname, xquote(path, "\n\r"),
			strerror(errno));
		return 1;
	}

	if (opt_name)
		err = print_attribute(path, opt_name, &header_printed);
	else
		err = list_attributes(path, &header_printed);

	if (header_printed)
		puts("");
	return err;
}

void help(void)
{
	printf(_("%s %s -- get extended attributes\n"),
	       progname, VERSION);
	printf(_("Usage: %s %s\n"),
	         progname, _(CMD_LINE_SPEC));
	printf(_(
"  -n, --name=name         get the named extended attribute value\n"
"  -d, --dump              get all extended attribute values\n"
"  -e, --encoding=...      encode values (as 'text', 'hex' or 'base64')\n"
"  -m, --match=pattern     only get attributes with names matching pattern\n"
"      --only-values       print the bare values only\n"
"  -h, --no-dereference    do not dereference symbolic links\n"
"      --one-file-system   skip files on different filesystems\n"
"      --absolute-names    don't strip leading '/' in pathnames\n"
"  -R, --recursive         recurse into subdirectories\n"
"  -L, --logical           logical walk, follow symbolic links\n"
"  -P  --physical          physical walk, do not follow symbolic links\n"
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
			case 'a': /* absolute names */
				opt_strip_leading_slash = 0;
				break;

			case 'd': /* dump attribute values */
				opt_dump = 1;
				break;

			case 'e':  /* encoding */
				if (strcmp(optarg, "text") != 0 &&
				    strcmp(optarg, "hex") != 0 &&
				    strcmp(optarg, "base64") != 0)
					goto synopsis;
				opt_encoding = optarg;
				break;

			case 'H':
				help();
				return 0;

			case 'h': /* do not dereference symlinks */
				walk_flags &= ~WALK_TREE_DEREFERENCE;
				break;

			case 'n':  /* get named attribute */
				opt_dump = 1;
				opt_name = optarg;
				break;

			case 'm':  /* regular expression for filtering names */
				opt_name_pattern = optarg;
				if (strcmp(opt_name_pattern, "-") == 0)
					opt_name_pattern = "";
				break;

			case 'v':  /* get attribute values only */
				opt_value_only = 1;
				break;

			case 'L':
				walk_flags |= WALK_TREE_LOGICAL;
				walk_flags &= ~WALK_TREE_PHYSICAL;
				break;

			case 'P':
				walk_flags |= WALK_TREE_PHYSICAL;
				walk_flags &= ~WALK_TREE_LOGICAL;
				break;

			case 'R':
				walk_flags |= WALK_TREE_RECURSIVE;
				break;

			case 1: /* one filesystem */
				walk_flags |= WALK_TREE_ONE_FILESYSTEM;
				break;

			case 'V':
				printf("%s " VERSION "\n", progname);
				return 0;

			case ':':  /* option missing */
			case '?':  /* unknown option */
			default:
				goto synopsis;
		}
	}
	if (optind >= argc)
		goto synopsis;

	if (regcomp(&name_regex, opt_name_pattern,
	            REG_EXTENDED | REG_NOSUB) != 0) {
		fprintf(stderr, _("%s: invalid regular expression \"%s\"\n"),
			progname, opt_name_pattern);
		return 1;
	}

	while (optind < argc) {
		had_errors += walk_tree(argv[optind], walk_flags, 0,
					do_print, NULL);
		optind++;
	}

	return (had_errors ? 1 : 0);

synopsis:
	fprintf(stderr, _("Usage: %s %s\n"
	                  "Try `%s --help' for more information.\n"),
		progname, CMD_LINE_SPEC, progname);
	return 2;
}

