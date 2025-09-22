/*
 * gen_uuid.c --- generate a DCE-compatible uuid
 *
 * Copyright (C) 1999, Andreas Dilger and Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "uuid.h"
#include "nls.h"
#include "c.h"
#include "closestream.h"
#include "strutils.h"
#include "optutils.h"
#include "xalloc.h"

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Create a new UUID value.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -r, --random          generate random-based uuid\n"), out);
	fputs(_(" -t, --time            generate time-based uuid\n"), out);
	fputs(_(" -n, --namespace <ns>  generate hash-based uuid in this namespace\n"), out);
	fprintf(out, _("                        available namespaces: %s\n"), "@dns @url @oid @x500");
	fputs(_(" -N, --name <name>     generate hash-based uuid from this name\n"), out);
	fputs(_(" -m, --md5             generate md5 hash\n"), out);
	fputs(_(" -C, --count <num>     generate more uuids in loop\n"), out);
	fputs(_(" -s, --sha1            generate sha1 hash\n"), out);
	fputs(_(" -x, --hex             interpret name as hex string\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(21));
	fprintf(out, USAGE_MAN_TAIL("uuidgen(1)"));
	exit(EXIT_SUCCESS);
}

static char *unhex(const char *value, size_t *valuelen)
{
	char c, *value2;
	unsigned n, x;

	if (*valuelen % 2 != 0) {
badstring:
		warnx(_("not a valid hex string"));
		errtryhelp(EXIT_FAILURE);
	}

	value2 = xmalloc(*valuelen / 2 + 1);

	for (x = n = 0; n < *valuelen; n++) {
		c = value[n];
		if ('0' <= c && c <= '9')
			x += c - '0';
		else if (('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'))
			x += (c - 'A' + 10) & 0xf;
		else
			goto badstring;

		if (n % 2 == 0)
			x *= 16;
		else {
			value2[n / 2] = x;
			x = 0;
		}
	}
	value2[n / 2] = '\0';

	*valuelen = (n / 2);

	return value2;
}

int
main (int argc, char *argv[])
{
	int    c;
	int    do_type = 0, is_hex = 0;
	char   str[UUID_STR_LEN];
	char   *namespace = NULL, *name = NULL;
	size_t namelen = 0;
	uuid_t ns, uu;
	unsigned int count = 1, i;

	static const struct option longopts[] = {
		{"random", no_argument, NULL, 'r'},
		{"time", no_argument, NULL, 't'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"namespace", required_argument, NULL, 'n'},
		{"name", required_argument, NULL, 'N'},
		{"md5", no_argument, NULL, 'm'},
		{"count", required_argument, NULL, 'C'},
		{"sha1", no_argument, NULL, 's'},
		{"hex", no_argument, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};

	static const ul_excl_t excl[] = {
		{ 'C', 'm', 's' },
		{ 'N', 'r', 't' },
		{ 'm', 'r', 's', 't' },
		{ 'n', 'r', 't' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "C:rtVhn:N:msx", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 't':
			do_type = UUID_TYPE_DCE_TIME;
			break;
		case 'r':
			do_type = UUID_TYPE_DCE_RANDOM;
			break;
		case 'n':
			namespace = optarg;
			break;
		case 'N':
			name = optarg;
			break;
		case 'm':
			do_type = UUID_TYPE_DCE_MD5;
			break;
		case 'C':
			count = strtou32_or_err(optarg, _("invalid count argument"));
			break;
		case 's':
			do_type = UUID_TYPE_DCE_SHA1;
			break;
		case 'x':
			is_hex = 1;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (namespace) {
		if (!name) {
			warnx(_("--namespace requires --name argument"));
			errtryhelp(EXIT_FAILURE);
		}
		if (do_type != UUID_TYPE_DCE_MD5 && do_type != UUID_TYPE_DCE_SHA1) {
			warnx(_("--namespace requires --md5 or --sha1"));
			errtryhelp(EXIT_FAILURE);
		}
	} else {
		if (name) {
			warnx(_("--name requires --namespace argument"));
			errtryhelp(EXIT_FAILURE);
		}
		if (do_type == UUID_TYPE_DCE_MD5 || do_type == UUID_TYPE_DCE_SHA1) {
			warnx(_("--md5 or --sha1 requires --namespace argument"));
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (name) {
		namelen = strlen(name);
		if (is_hex)
			name = unhex(name, &namelen);
	}

	for (i = 0; i < count; i++) {
		switch (do_type) {
		case UUID_TYPE_DCE_TIME:
			uuid_generate_time(uu);
			break;
		case UUID_TYPE_DCE_RANDOM:
			uuid_generate_random(uu);
			break;
		case UUID_TYPE_DCE_MD5:
		case UUID_TYPE_DCE_SHA1:
			if (namespace[0] == '@' && namespace[1] != '\0') {
				const uuid_t *uuidptr;

				uuidptr = uuid_get_template(&namespace[1]);
				if (uuidptr == NULL) {
					warnx(_("unknown namespace alias: '%s'"), namespace);
					errtryhelp(EXIT_FAILURE);
				}
				memcpy(ns, *uuidptr, sizeof(ns));
			} else {
				if (uuid_parse(namespace, ns) != 0) {
					warnx(_("invalid uuid for namespace: '%s'"), namespace);
					errtryhelp(EXIT_FAILURE);
				}
			}
			if (do_type == UUID_TYPE_DCE_MD5)
				uuid_generate_md5(uu, ns, name, namelen);
			else
				uuid_generate_sha1(uu, ns, name, namelen);
			break;
		default:
			uuid_generate(uu);
			break;
		}

		uuid_unparse(uu, str);

		printf("%s\n", str);
	}

	if (is_hex)
		free(name);

	return EXIT_SUCCESS;
}
