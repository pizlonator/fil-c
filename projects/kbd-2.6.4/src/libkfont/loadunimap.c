// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * Copyright (C) 2007-2020 Alexey Gladkov <gladkov.alexey@gmail.com>
 * Copyright (C) 2020 Oleg Bulatov <oleg@bulatov.me>
 *
 * Originally written by Andries Brouwer
 */
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#include <kbdfile.h>

#include "paths.h"
#include "libcommon.h"
#include "kfontP.h"
#include "utf8.h"

static int
str_to_ushort(struct kfont_context *ctx,
		const char *p, char **e, int base,
		unsigned short *res)
{
	long int v;

	errno = 0;
	v = strtol(p, e, base);

	if (e && p == *e) {
		KFONT_ERR(ctx, "Unable to parse number: %s", p);
		return -EX_DATAERR;
	}

	if (errno == ERANGE) {
		KFONT_ERR(ctx, "Numerical result out of range: %s", p);
		return -EX_DATAERR;
	}

	if (v < 0) {
		KFONT_ERR(ctx, "Number must not be negative: %ld", v);
		return -EX_DATAERR;
	}

	if (v > USHRT_MAX) {
		KFONT_ERR(ctx, "Number too big: %ld", v);
		return -EX_DATAERR;
	}

	*res = (unsigned short) v;
	return 0;
}

/*
 * Skip spaces and read U+1234 or return -1 for error.
 * Return first non-read position in *p0 (unchanged on error).
 */
static int
getunicode(struct kfont_context *ctx, char **p0, unsigned short *res)
{
	char *p = *p0;

	while (*p == ' ' || *p == '\t')
		p++;
#if 0
	/* The code below also allows one to accept 'utf8' */
	if (*p == '\'') {
		int err;
		unsigned long u;
		char *p1 = p+1;

		/*
		 * Read a single complete utf-8 character, and
		 * expect it to be closed by a single quote.
		 */
		u = from_utf8(&p1, 0, &err);
		if (err || *p1 != '\'')
			return -1;
		*p0 = p1+1;
		return u;
	}
#endif
	if (*p != 'U' || p[1] != '+' || !isxdigit(p[2]) || !isxdigit(p[3]) ||
	    !isxdigit(p[4]) || !isxdigit(p[5]) || isxdigit(p[6]))
		return -1;
	*p0 = p + 6;

	return str_to_ushort(ctx, p + 2, 0, 16, res);
}

static int
add_unipair(struct kfont_context *ctx,
		unsigned short fp, unsigned short un,
		struct unipair **list, unsigned int *listsz,
		unsigned short *listct)
{
	if (*listct == *listsz) {
		*listsz += 4096;
		*list = realloc(*list, *listsz);
		if (!*list) {
			KFONT_ERR(ctx, "realloc: %m");
			return -EX_OSERR;
		}
	}

	(*list)[*listct].fontpos = fp;
	(*list)[*listct].unicode = un;
	(*listct)++;

	return 0;
}

/*
 * Syntax accepted:
 *	<fontpos>	<unicode> <unicode> ...
 *	<range>		idem
 *	<range>		<unicode>
 *	<range>		<unicode range>
 *
 * where <range> ::= <fontpos>-<fontpos>
 * and <unicode> ::= U+<h><h><h><h>
 * and <h> ::= <hexadecimal digit>
 */

static int
parseline(struct kfont_context *ctx, char *buffer, const char *tblname,
		struct unipair **list, unsigned int *listsz,
		unsigned short *listct)
{
	int fontlen = 512;
	int ret;
	unsigned short i, fp0, fp1, un0, un1;
	char *p, *p1;

	p = buffer;

	while (*p == ' ' || *p == '\t')
		p++;
	if (!*p || *p == '#')
		return 0; /* skip comment or blank line */

	ret = str_to_ushort(ctx, p, &p1, 0, &fp0);
	if (ret < 0)
		return ret;
	p = p1;

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '-') {
		p++;

		ret = str_to_ushort(ctx, p, &p1, 0, &fp1);
		if (ret < 0)
			return ret;
		p = p1;
	} else
		fp1 = 0;

	if (fp0 >= fontlen) {
		KFONT_ERR(ctx, _("%s: Glyph number (0x%x) larger than font length"),
		    tblname, fp0);
		return -EX_DATAERR;
	}

	if (fp1 && (fp1 < fp0 || fp1 >= fontlen)) {
		KFONT_ERR(ctx, _("%s: Bad end of range (0x%x)\n"),
		    tblname, fp1);
		return -EX_DATAERR;
	}

	if (fp1) {
		/* we have a range; expect the word "idem" or a Unicode range
		   of the same length or a single Unicode value */
		while (*p == ' ' || *p == '\t')
			p++;
		if (!strncmp(p, "idem", 4)) {
			p += 4;
			for (i = fp0; i <= fp1; i++) {
				if ((ret = add_unipair(ctx, i, i, list, listsz, listct)) < 0)
					return ret;
			}
			goto lookattail;
		}

		if ((ret = getunicode(ctx, &p, &un0)) < 0)
			return ret;

		while (*p == ' ' || *p == '\t')
			p++;
		if (*p != '-') {
			for (i = fp0; i <= fp1; i++) {
				if ((ret = add_unipair(ctx, i, un0, list, listsz, listct)) < 0)
					return ret;
			}
			goto lookattail;
		}

		p++;

		if ((ret = getunicode(ctx, &p, &un1)) < 0)
			return ret;

		if (un1 - un0 != fp1 - fp0) {
			KFONT_ERR(ctx,
			        _("%s: Unicode range U+%x-U+%x not of the same"
			          " length as font position range 0x%x-0x%x"),
			        tblname, un0, un1, fp0, fp1);
			return -EX_DATAERR;
		}

		for (i = fp0; i <= fp1; i++) {
			unsigned short un = un0 - fp0;
			if ((ret = add_unipair(ctx, i, un + i, list, listsz, listct)) < 0)
				return ret;
		}

	} else {
		/* no range; expect a list of unicode values
		   for a single font position */

		while (!getunicode(ctx, &p, &un0)) {
			if ((ret = add_unipair(ctx, fp0, un0, list, listsz, listct)) < 0)
				return ret;
		}
	}
lookattail:
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p && *p != '#')
		KFONT_ERR(ctx, _("%s: trailing junk (%s) ignored"), tblname, p);

	return 0;
}

int
kfont_load_unicodemap(struct kfont_context *ctx, int fd, const char *tblname)
{
	char buffer[65536];
	char *p;
	struct kbdfile *fp;
	struct unimapdesc descr;
	struct unipair *list = NULL;
	unsigned int listsz  = 0;
	unsigned short listct  = 0;

	int ret = 0;

	if (!(fp = kbdfile_new(NULL))) {
		KFONT_ERR(ctx, "Unable to create kbdfile instance: %m");
		return -EX_OSERR;
	}

	if (kbdfile_find(tblname, ctx->unidirpath, ctx->unisuffixes, fp)) {
		KFONT_ERR(ctx, "unable to find unimap: %s: %m", tblname);
		ret = -EX_NOINPUT;
		goto err;
	}

	KFONT_INFO(ctx, _("Loading unicode map from file %s"), kbdfile_get_pathname(fp));

	while (fgets(buffer, sizeof(buffer), kbdfile_get_file(fp)) != NULL) {
		if ((p = strchr(buffer, '\n')) != NULL)
			*p = '\0';
		else
			KFONT_WARN(ctx, _("%s: Warning: line too long"), tblname);

		if ((ret = parseline(ctx, buffer, tblname, &list, &listsz, &listct)) < 0)
			goto err;
	}

	if (listct == 0 && !(ctx->options & (1 << kfont_force))) {
		KFONT_ERR(ctx,
		        _("not loading empty unimap\n"
		          "(if you insist: use option -f to override)"));
	} else {
		descr.entry_ct = listct;
		descr.entries  = list;
		if ((ret = kfont_put_unicodemap(ctx, fd, NULL, &descr)) < 0)
			goto err;
		listct = 0;
	}
err:
	kbdfile_free(fp);
	free(list);

	return ret;
}

static int
getunicodemap(struct kfont_context *ctx, int fd, struct unimapdesc *unimap_descr)
{
	if (kfont_get_unicodemap(ctx, fd, unimap_descr))
		return -1;

	KFONT_INFO(ctx, P_("# %d entry", "# %d entries", unimap_descr->entry_ct),
			unimap_descr->entry_ct);
	return 0;
}

int
kfont_save_unicodemap(struct kfont_context *ctx, int consolefd,
		const char *filename)
{
	FILE *fpo;
	struct unimapdesc unimap_descr = { 0 };
	struct unipair *unilist;
	int i, ret;

	if ((fpo = fopen(filename, "w")) == NULL) {
		KFONT_ERR(ctx, _("Unable to open file: %s: %m"), filename);
		return -EX_DATAERR;
	}

	if ((ret = getunicodemap(ctx, consolefd, &unimap_descr)) < 0)
		goto end;

	unilist = unimap_descr.entries;

	for (i = 0; i < unimap_descr.entry_ct; i++)
		fprintf(fpo, "0x%02x\tU+%04x\n", unilist[i].fontpos, unilist[i].unicode);

	KFONT_INFO(ctx, _("Saved unicode map on `%s'"), filename);
end:
	fclose(fpo);
	return ret;
}

int
appendunicodemap(struct kfont_context *ctx, int fd, FILE *fp,
		unsigned int fontsize, int utf8)
{
	struct unimapdesc unimap_descr = { 0 };
	struct unipair *unilist;
	unsigned int i;
	int j, ret;

	if ((ret = getunicodemap(ctx, fd, &unimap_descr)) < 0)
		return ret;

	unilist = unimap_descr.entries;

	for (i = 0; i < fontsize; i++) {
#if 0
		/* More than one mapping is not a sequence! */
		int no = 0;
		for(j=0; j<unimap_descr.entry_ct; j++)
			if (unilist[j].fontpos == i)
				no++;
		if (no > 1)
			appendseparator(fp, 1, utf8);
#endif
		if (ctx->verbose > 1)
			printf("\nchar %03x: ", i);

		for (j = 0; j < unimap_descr.entry_ct; j++) {
			if (unilist[j].fontpos == i) {
				if (ctx->verbose > 1)
					printf("%04x ", unilist[j].unicode);
				if ((ret = appendunicode(ctx,fp, unilist[j].unicode, utf8)) < 0)
					return ret;
			}
		}

		if ((ret = appendseparator(ctx, fp, 0, utf8)) < 0)
			return ret;
	}

	if (ctx->verbose > 1)
		printf("\n");

	KFONT_INFO(ctx, _("Appended Unicode map"));

	return 0;
}
