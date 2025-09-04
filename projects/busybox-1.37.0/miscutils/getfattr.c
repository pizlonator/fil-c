/*
 * getfattr - get extended attributes of filesystem objects.
 *
 * Copyright (C) 2023 by LoveSy <lovesykun@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config GETFATTR
//config:	bool "getfattr (12.3 kb)"
//config:	default y
//config:	help
//config:	Get extended attributes on files

//applet:IF_GETFATTR(APPLET_NOEXEC(getfattr, getfattr, BB_DIR_USR_BIN, BB_SUID_DROP, getfattr))

//kbuild:lib-$(CONFIG_GETFATTR) += getfattr.o

#include <stdio.h>
#include <sys/xattr.h>
#include "libbb.h"

//usage:#define getfattr_trivial_usage
//usage:       "[-h] {-d|-n ATTR} FILE...\n"
//usage:#define getfattr_full_usage "\n\n"
//usage:       "Get extended attributes"
//usage:     "\n"
//usage:     "\n	-h		Do not follow symlinks"
//usage:     "\n	-d		Dump all attributes"
//usage:     "\n	-n ATTR		Get attribute ATTR"

enum {
	OPT_h = (1 << 0),
	OPT_d = (1 << 1),
	OPT_n = (1 << 2),
};

static int print_attr(const char *file, const char *name, char **buf, size_t *bufsize)
{
	ssize_t len;

	if (*bufsize == 0)
		goto grow;
 again:
	len = ((option_mask32 &  OPT_h) ? lgetxattr: getxattr)(file, name, *buf, *bufsize);
	if (len < 0) {
		if (errno != ERANGE)
			return len;
 grow:
		*bufsize = (*bufsize * 2) + 1024;
		*buf = xrealloc(*buf, *bufsize);
		goto again;
	}
	printf("%s=\"%.*s\"\n", name, len, *buf);
	return 0;
}

static ssize_t list_attr(const char *file, char **list, size_t *listsize)
{
	ssize_t len;

	if (*listsize == 0)
		goto grow;
 again:
	len = ((option_mask32 &  OPT_h) ? llistxattr : listxattr)(file, *list, *listsize);
	if (len < 0) {
		if (errno != ERANGE)
			return len;
 grow:
		*listsize = (*listsize * 2) + 1024;
		*list = xrealloc(*list, *listsize);
		goto again;
	}
	return len;
}

int getfattr_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int getfattr_main(int argc UNUSED_PARAM, char **argv)
{
	const char *name;
	exitcode_t status;
	int opt;
	char *buf = NULL;
	size_t bufsize = 0;
	char *list = NULL;
	size_t listsize = 0;

	opt = getopt32(argv, "^"
		"hdn:"
		/* Min one arg; -d and -n are exclusive */
		"\0" "-1:n--d:d--n"
			//getfattr 2.5.1 does not enforce this: ":d:n" /* exactly one of -n or -d is required */
		, &name
	);
	argv += optind;
	status = EXIT_SUCCESS;

	do {
		int r;
//getfattr 2.5.1 with no -n/-d defaults to -d
		if (!(opt & OPT_n)) {
			ssize_t len = list_attr(*argv, &list, &listsize);
			if (len < 0)
				goto err;
			if (len > 0) {
				char *key;
				printf("# file: %s\n", *argv);
				key = list;
				while (len > 0) {
					ssize_t keylen;
					r = print_attr(*argv, key, &buf, &bufsize);
					if (r)
						goto err;
					keylen = strlen(key) + 1;
					key += keylen;
					len -= keylen;
				}
				bb_putchar('\n');
			}
		} else {
			printf("# file: %s\n", *argv);
			r = print_attr(*argv, name, &buf, &bufsize);
			if (r) {
 err:
				bb_simple_perror_msg(*argv);
				status = EXIT_FAILURE;
				continue;
			}
			bb_putchar('\n');
		}
	} while (*++argv);

	if (ENABLE_FEATURE_CLEAN_UP)
		free(buf);

	fflush_stdout_and_exit(status);
}
