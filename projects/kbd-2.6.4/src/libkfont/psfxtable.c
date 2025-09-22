// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * Copyright (C) 2007-2020 Alexey Gladkov <gladkov.alexey@gmail.com>
 * Copyright (C) 1999 Andries E. Brouwer
 *  derived from sources that were
 * Copyright (C) 1994 H. Peter Anvin
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sysexits.h>
#include <limits.h>
#include <errno.h>

#include "libcommon.h"
#include "kfontP.h"

static int
str_to_unicode(struct kfont_context *ctx,
		const char *p, char **e, int base,
		unicode *res)
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

	if (v > INT_MAX) {
		KFONT_ERR(ctx, "Number too big: %ld", v);
		return -EX_DATAERR;
	}

	*res = (unicode) v;
	return 0;
}

/*
 * Parse humanly readable unicode table.
 * Format: lines
 *   <fontposition><tab><uc_defs>
 * or
 *   <fontrange><tab><uc_defs>
 * or
 *   <fontrange><tab><uc_range>
 *  where
 *   <uc_defs> :: <empty> | <uc_def><space><uc_defs>
 *   <uc_def> :: <uc> | <uc>,<uc_def>
 *   <uc> :: U+<h><h><h><h>
 *   <h> :: <hexadecimal digit>
 *   <range> :: <value>-<value>
 *  Blank lines and lines starting with # are ignored.
 */

static unicode
getunicode(struct kfont_context *ctx, char **p0, unicode *res)
{
	char *p = *p0;

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != 'U' || p[1] != '+' ||
			!isxdigit(p[2]) || !isxdigit(p[3]) || !isxdigit(p[4]) ||
			!isxdigit(p[5]) || isxdigit(p[6]))
		return -1;
	*p0 = p + 6;

	return str_to_unicode(ctx, p + 2, 0, 16, res);
}

static int
parse_itab_line(struct kfont_context *ctx, char *buf, unsigned int fontlen,
		struct unicode_list *uclistheads)
{
	char *p, *p1;
	int ret;
	unicode i, fp0, fp1, un0, un1;

	if ((p = strchr(buf, '\n')) != NULL)
		*p = 0;
	else {
		KFONT_ERR(ctx, _("Warning: line too long"));
		return -EX_DATAERR;
	}

	p = buf;

	while (*p == ' ' || *p == '\t')
		p++;
	if (!*p || *p == '#')
		return 0;

	ret = str_to_unicode(ctx, p, &p1, 0, &fp0);
	if (ret < 0)
		return ret;
	p = p1;

	if (*p == '-') {
		p++;

		ret = str_to_unicode(ctx, p, &p1, 0, &fp1);
		if (ret < 0)
			return ret;
		p = p1;
	} else
		fp1 = 0;

	if ((unsigned int) fp0 >= fontlen) {
		KFONT_ERR(ctx, _("Glyph number (0x%x) past end of font"), fp0);
		return -EX_DATAERR;
	}
	if (fp1 && (fp1 < fp0 || (unsigned int) fp1 >= fontlen)) {
		KFONT_ERR(ctx, _("Bad end of range (0x%x)"), fp1);
		return -EX_DATAERR;
	}

	if (fp1) {
		/* we have a range; expect the word "idem"
		   or a Unicode range of the same length */
		while (*p == ' ' || *p == '\t')
			p++;
		if (!strncmp(p, "idem", 4)) {
			for (i = fp0; i <= fp1; i++) {
				ret = addpair(uclistheads + i, i);
				if (ret < 0) {
					KFONT_ERR(ctx, "unable to add pair: %s", strerror(-ret));
					return -EX_OSERR;
				}
			}
			p += 4;
		} else {
			if ((ret = getunicode(ctx, &p, &un0)) < 0)
				return ret;

			while (*p == ' ' || *p == '\t')
				p++;
			if (*p != '-') {
				KFONT_ERR(ctx,
				        _("Corresponding to a range of "
				          "font positions, there should be "
				          "a Unicode range"));
				return -EX_DATAERR;
			}
			p++;

			if ((ret = getunicode(ctx, &p, &un1)) < 0)
				return ret;

			if (un1 - un0 != fp1 - fp0) {
				KFONT_ERR(ctx,
				        _("Unicode range U+%x-U+%x not "
				          "of the same length as font "
				          "position range 0x%x-0x%x"),
				        un0, un1, fp0, fp1);
				return -EX_DATAERR;
			}
			for (i = fp0; i <= fp1; i++) {
				ret = addpair(uclistheads + i, un0 - fp0 + i);
				if (ret < 0) {
					KFONT_ERR(ctx, "unable to add pair: %s", strerror(-ret));
					return -EX_OSERR;
				}
			}
		} /* not idem */
	} else {  /* no range */
		while (!getunicode(ctx, &p, &un0)) {
			if ((ret = addpair(uclistheads + fp0, un0)) < 0) {
				KFONT_ERR(ctx, "unable to add pair: %s", strerror(-ret));
				return -EX_OSERR;
			}

			while (*p++ == ',' && !getunicode(ctx, &p, &un1)) {
				if ((ret = addseq(uclistheads + fp0, un1)) < 0) {
					KFONT_ERR(ctx, "unable to add sequence: %s", strerror(-ret));
					return -EX_OSERR;
				}
			}
			p--;
		}

		while (*p == ' ' || *p == '\t')
			p++;

		if (*p && *p != '#') {
			KFONT_ERR(ctx, _("trailing junk (%s) ignored"), p);
		}
	}

	return 0;
}

int
kfont_read_unicodetable(struct kfont_context *ctx, FILE *file,
		unsigned int fontlen,
		struct unicode_list **uclistheadsp)
{
	char buf[65536];
	unsigned int i;

	*uclistheadsp = realloc(*uclistheadsp, fontlen * sizeof(struct unicode_list));
	if (!*uclistheadsp) {
		KFONT_ERR(ctx, "realloc: %m");
		return -EX_OSERR;
	}

	for (i = 0; i < fontlen; i++) {
		struct unicode_list *up = &((*uclistheadsp)[i]);
		up->next = NULL;
		up->seq  = NULL;
		up->prev = up;
	}

	while (fgets(buf, sizeof(buf), file) != NULL) {
		int ret = parse_itab_line(ctx, buf, fontlen, *uclistheadsp);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int
kfont_write_unicodetable(struct kfont_context *ctx, FILE *file,
		unsigned int fontlen,
		struct unicode_list *uclistheads)
{
	struct unicode_list *ul;
	struct unicode_seq *us;
	const char *sep;
	unsigned int i;

	if (fprintf(file, "#\n# Character table extracted from font\n#\n") < 0) {
		KFONT_ERR(ctx, "Unable to write unicode table");
		return -EX_IOERR;
	}

	for (i = 0; i < fontlen; i++) {
		if (fprintf(file, "0x%03x\t", i) < 0) {
			KFONT_ERR(ctx, "Unable to write unicode table");
			return -EX_IOERR;
		}

		sep = "";
		ul  = uclistheads[i].next;
		while (ul) {
			us = ul->seq;
			while (us) {
				if (fprintf(file, "%sU+%04x", sep, us->uc) < 0) {
					KFONT_ERR(ctx, "Unable to write unicode table");
					return -EX_IOERR;
				}

				us  = us->next;
				sep = ", ";
			}
			ul  = ul->next;
			sep = " ";
		}

		if (fprintf(file, "\n") < 0) {
			KFONT_ERR(ctx, "Unable to write unicode table");
			return -EX_IOERR;
		}
	}

	return 0;
}
