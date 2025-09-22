/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file is part of util-linux.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * The original namei(1) was written by:
 *	Roger S. Southwick (May 2, 1990)
 *	Steve Tell (March 28, 1991)
 *	Arkadiusz Miśkiewicz (1999-02-22)
 *	Li Zefan (2007-09-10).
 */
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_LIBSELINUX
# include <selinux/selinux.h>
#endif

#include "c.h"
#include "xalloc.h"
#include "nls.h"
#include "widechar.h"
#include "strutils.h"
#include "closestream.h"
#include "idcache.h"

#ifndef MAXSYMLINKS
#define MAXSYMLINKS 256
#endif

#define NAMEI_NOLINKS	(1 << 1)
#define NAMEI_MODES	(1 << 2)
#define NAMEI_MNTS	(1 << 3)
#define NAMEI_OWNERS	(1 << 4)
#define NAMEI_VERTICAL	(1 << 5)
#define NAMEI_CONTEXT	(1 << 6)


struct namei {
	struct stat	st;		/* item lstat() */
	char		*name;		/* item name */
	char		*abslink;	/* absolute symlink path */
	int		relstart;	/* offset of relative path in 'abslink' */
	struct namei	*next;		/* next item */
	int		level;
	int		mountpoint;	/* is mount point */
	int		noent;		/* this item not existing (stores errno from stat()) */
#ifdef HAVE_LIBSELINUX
	int		context_len;	/* length of selinux contexts, as returned by lgetfilecon(3) */
	char		*context;	/* selinux contexts, as set by lgetfilecon(3) */
#endif
};

static int flags;
static struct idcache *gcache;	/* groupnames */
static struct idcache *ucache;	/* usernames */

static void
free_namei(struct namei *nm)
{
	while (nm) {
		struct namei *next = nm->next;
#ifdef HAVE_LIBSELINUX
		free(nm->context);
#endif
		free(nm->name);
		free(nm->abslink);
		free(nm);
		nm = next;
	}
}

static void
readlink_to_namei(struct namei *nm, const char *path)
{
	char sym[PATH_MAX];
	ssize_t sz;
	int isrel = 0;

	sz = readlink(path, sym, sizeof(sym));
	if (sz < 1)
		err(EXIT_FAILURE, _("failed to read symlink: %s"), path);
	if (*sym != '/') {
		char *p = strrchr(path, '/');

		if (p) {
			isrel = 1;
			nm->relstart = p - path;
			sz += nm->relstart + 1;
		}
	}
	nm->abslink = xmalloc(sz + 1);

	if (isrel) {
		/* create the absolute path from the relative symlink */
		memcpy(nm->abslink, path, nm->relstart);
		*(nm->abslink + nm->relstart) = '/';
		nm->relstart++;
		memcpy(nm->abslink + nm->relstart, sym, sz - nm->relstart);
	} else
		/* - absolute link (foo -> /path/bar)
		 * - or link without any subdir (foo -> bar)
		 */
		memcpy(nm->abslink, sym, sz);

	nm->abslink[sz] = '\0';
}

static struct stat *
dotdot_stat(const char *dirname, struct stat *st)
{
	char *path;
	size_t len;

#define DOTDOTDIR	"/.."

	if (!dirname)
		return NULL;

	len = strlen(dirname);
	path = xmalloc(len + sizeof(DOTDOTDIR));

	memcpy(path, dirname, len);
	memcpy(path + len, DOTDOTDIR, sizeof(DOTDOTDIR));

	if (stat(path, st))
		err(EXIT_FAILURE, _("stat of %s failed"), path);
	free(path);
	return st;
}

static struct namei *
new_namei(struct namei *parent, const char *path, const char *fname, int lev)
{
	struct namei *nm;

	if (!fname)
		return NULL;
	nm = xcalloc(1, sizeof(*nm));
	if (parent)
		parent->next = nm;

	nm->level = lev;
	nm->name = xstrdup(fname);

#ifdef HAVE_LIBSELINUX
	/* Don't use is_selinux_enabled() here. We need info about a context
	 * also on systems where SELinux is (temporary) disabled */
	nm->context_len = lgetfilecon(path, &nm->context);
#endif
	if (lstat(path, &nm->st) != 0) {
		nm->noent = errno;
		return nm;
	}

	if (S_ISLNK(nm->st.st_mode))
		readlink_to_namei(nm, path);
	if (flags & NAMEI_OWNERS) {
		add_uid(ucache, nm->st.st_uid);
		add_gid(gcache, nm->st.st_gid);
	}

	if ((flags & NAMEI_MNTS) && S_ISDIR(nm->st.st_mode)) {
		struct stat stbuf, *sb = NULL;

		if (parent && S_ISDIR(parent->st.st_mode))
			sb = &parent->st;
		else if (!parent || S_ISLNK(parent->st.st_mode))
			sb = dotdot_stat(path, &stbuf);

		if (sb && (sb->st_dev != nm->st.st_dev ||   /* different device */
		           sb->st_ino == nm->st.st_ino))    /* root directory */
			nm->mountpoint = 1;
	}

	return nm;
}

static struct namei *
add_namei(struct namei *parent, const char *orgpath, int start, struct namei **last)
{
	struct namei *nm = NULL, *first = NULL;
	char *fname, *end, *path;
	int level = 0;

	if (!orgpath)
		return NULL;
	if (parent) {
		nm = parent;
		level = parent->level + 1;
	}
	path = xstrdup(orgpath);
	fname = path + start;

	/* root directory */
	if (*fname == '/') {
		while (*fname == '/')
			fname++; /* eat extra '/' */
		first = nm = new_namei(nm, "/", "/", level);
	}

	for (end = fname; fname && end; ) {
		/* set end of filename */
		if (*fname) {
			end = strchr(fname, '/');
			if (end)
				*end = '\0';

			/* create a new entry */
			nm = new_namei(nm, path, fname, level);
		} else
			end = NULL;
		if (!first)
			first = nm;
		/* set begin of the next filename */
		if (end) {
			*end++ = '/';
			while (*end == '/')
				end++; /* eat extra '/' */
		}
		fname = end;
	}

	if (last)
		*last = nm;

	free(path);

	return first;
}

static int
follow_symlinks(struct namei *nm)
{
	int symcount = 0;

	for (; nm; nm = nm->next) {
		struct namei *next, *last;

		if (nm->noent)
			continue;
		if (!S_ISLNK(nm->st.st_mode))
			continue;
		if (++symcount > MAXSYMLINKS) {
			/* drop the rest of the list */
			free_namei(nm->next);
			nm->next = NULL;
			return -1;
		}
		next = nm->next;
		nm->next = add_namei(nm, nm->abslink, nm->relstart, &last);
		if (last)
			last->next = next;
		else
			nm->next = next;
	}
	return 0;
}

static int
print_namei(struct namei *nm, char *path)
{
	int i;

	if (path)
		printf("f: %s\n", path);

	for (; nm; nm = nm->next) {
		char md[11];

		if (nm->noent) {
			int blanks = 1;
			if (flags & NAMEI_MODES)
				blanks += 9;
			if (flags & NAMEI_OWNERS)
				blanks += ucache->width + gcache->width + 2;
			if (!(flags & NAMEI_VERTICAL))
				blanks += 1;
			if (!(flags & NAMEI_CONTEXT))
				blanks += 1;
			blanks += nm->level * 2;
			printf("%*s ", blanks, "");
			printf("%s - %s\n", nm->name, strerror(nm->noent));
			return -1;
		}

		xstrmode(nm->st.st_mode, md);

		if (nm->mountpoint)
			md[0] = 'D';

		if (!(flags & NAMEI_VERTICAL)) {
			for (i = 0; i < nm->level; i++)
				fputs("  ", stdout);
			fputc(' ', stdout);
		}

		if (flags & NAMEI_MODES)
			printf("%s", md);
		else
			printf("%c", md[0]);

		if (flags & NAMEI_OWNERS) {
			printf(" %-*s", ucache->width,
				get_id(ucache, nm->st.st_uid)->name);
			printf(" %-*s", gcache->width,
				get_id(gcache, nm->st.st_gid)->name);
		}
#ifdef HAVE_LIBSELINUX
		if (flags & NAMEI_CONTEXT) {
			if (nm->context)
				printf(" %-*s", nm->context_len, nm->context);
			else
				printf(" ?");
		}
#endif
		if (flags & NAMEI_VERTICAL)
			for (i = 0; i < nm->level; i++)
				fputs("  ", stdout);

		if (S_ISLNK(nm->st.st_mode))
			printf(" %s -> %s\n", nm->name,
					nm->abslink + nm->relstart);
		else
			printf(" %s\n", nm->name);
	}
	return 0;
}

static void __attribute__((__noreturn__)) usage(void)
{
	const char *p = program_invocation_short_name;
	FILE *out = stdout;

	if (!*p)
		p = "namei";

	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options] <pathname>...\n"), p);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Follow a pathname until a terminal point is found.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(
		" -x, --mountpoints   show mount point directories with a 'D'\n"
		" -m, --modes         show the mode bits of each file\n"
		" -o, --owners        show owner and group name of each file\n"
		" -l, --long          use a long listing format (-m -o -v) \n"
		" -n, --nosymlinks    don't follow symlinks\n"
		" -v, --vertical      vertical align of modes and owners\n"), out);
#ifdef HAVE_LIBSELINUX
	fputs(_( " -Z, --context       print any security context of each file \n"), out);
#endif

	fprintf(out, USAGE_HELP_OPTIONS(21));

	fprintf(out, USAGE_MAN_TAIL("namei(1)"));
	exit(EXIT_SUCCESS);
}

static const struct option longopts[] =
{
	{ "help",	 no_argument, NULL, 'h' },
	{ "version",     no_argument, NULL, 'V' },
	{ "mountpoints", no_argument, NULL, 'x' },
	{ "modes",	 no_argument, NULL, 'm' },
	{ "owners",	 no_argument, NULL, 'o' },
	{ "long",        no_argument, NULL, 'l' },
	{ "nolinks",	 no_argument, NULL, 'n' },
	{ "vertical",    no_argument, NULL, 'v' },
#ifdef HAVE_LIBSELINUX
	{ "context",	 no_argument, NULL, 'Z' },
#endif
	{ NULL, 0, NULL, 0 },
};

int
main(int argc, char **argv)
{
	int c;
	int rc = EXIT_SUCCESS;
	static const char *shortopts =
#ifdef HAVE_LIBSELINUX
		"Z"
#endif
		"hVlmnovx";

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch(c) {
		case 'l':
			flags |= (NAMEI_OWNERS | NAMEI_MODES | NAMEI_VERTICAL);
			break;
		case 'm':
			flags |= NAMEI_MODES;
			break;
		case 'n':
			flags |= NAMEI_NOLINKS;
			break;
		case 'o':
			flags |= NAMEI_OWNERS;
			break;
		case 'x':
			flags |= NAMEI_MNTS;
			break;
		case 'v':
			flags |= NAMEI_VERTICAL;
			break;
#ifdef HAVE_LIBSELINUX
		case 'Z':
			flags |= NAMEI_CONTEXT;
			break;
#endif
		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind == argc) {
		warnx(_("pathname argument is missing"));
		errtryhelp(EXIT_FAILURE);
	}

	ucache = new_idcache();
	if (!ucache)
		err(EXIT_FAILURE, _("failed to allocate UID cache"));
	gcache = new_idcache();
	if (!gcache)
		err(EXIT_FAILURE, _("failed to allocate GID cache"));

	for(; optind < argc; optind++) {
		char *path = argv[optind];
		struct namei *nm = NULL;
		struct stat st;

		if (stat(path, &st) != 0)
			rc = EXIT_FAILURE;

		nm = add_namei(NULL, path, 0, NULL);
		if (nm) {
			int sml = 0;
			if (!(flags & NAMEI_NOLINKS))
				sml = follow_symlinks(nm);
			if (print_namei(nm, path)) {
				rc = EXIT_FAILURE;
				continue;
			}
			free_namei(nm);
			if (sml == -1) {
				rc = EXIT_FAILURE;
				warnx(_("%s: exceeded limit of symlinks"), path);
				continue;
			}
		}
	}

	free_idcache(ucache);
	free_idcache(gcache);

	return rc;
}
