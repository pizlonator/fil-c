/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 /* 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  */

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "hexdump.h"
#include "nls.h"
#include "xalloc.h"
#include "strutils.h"
#include "colors.h"

static void escape(char *p1);
static struct list_head *color_fmt(char *cfmt, int bcnt);

static void __attribute__ ((__noreturn__)) badcnt(const char *s)
{
        errx(EXIT_FAILURE, _("bad byte count for conversion character %s"), s);
}

static void __attribute__ ((__noreturn__)) badsfmt(void)
{
        errx(EXIT_FAILURE, _("%%s requires a precision or a byte count"));
}

static void __attribute__ ((__noreturn__)) badfmt(const char *fmt)
{
        errx(EXIT_FAILURE, _("bad format {%s}"), fmt);
}

static void __attribute__ ((__noreturn__)) badconv(const char *ch)
{
        errx(EXIT_FAILURE, _("bad conversion character %%%s"), ch);
}

#define first_letter(s,f) strchr(f, *(s))

struct hexdump_fu *endfu;					/* format at end-of-data */

void addfile(char *name, struct hexdump *hex)
{
	char *fmt, *buf = NULL;
	FILE *fp;
	size_t n = 0;

	if ((fp = fopen(name, "r")) == NULL)
	        err(EXIT_FAILURE, _("can't read %s"), name);

	while (getline(&buf, &n, fp) != -1) {
		fmt = buf;

		while (*fmt && isspace(*fmt))
			++fmt;
		if (!*fmt || *fmt == '#')
			continue;

		add_fmt(fmt, hex);
	}

	free(buf);
	fclose(fp);
}

static char *next_number(const char *str, int *num)
{
	char *end = NULL;

	errno = 0;
	*num = strtol(str, &end, 10);

	if (errno || !end || end == str)
		return NULL;
	return end;
}

void add_fmt(const char *fmt, struct hexdump *hex)
{
	const char *p, *savep;
	struct hexdump_fs *tfs;
	struct hexdump_fu *tfu;

	/* Start new linked list of format units. */
	tfs = xcalloc(1, sizeof(struct hexdump_fs));
	INIT_LIST_HEAD(&tfs->fslist);
	INIT_LIST_HEAD(&tfs->fulist);
	list_add_tail(&tfs->fslist, &hex->fshead);

	/* Take the format string and break it up into format units. */
	p = fmt;
	while (TRUE) {
		/* Skip leading white space. */
		if (!*(p = skip_space(p)))
			break;

		/* Allocate a new format unit and link it in. */
		tfu = xcalloc(1, sizeof(struct hexdump_fu));
		tfu->reps = 1;

		INIT_LIST_HEAD(&tfu->fulist);
		INIT_LIST_HEAD(&tfu->prlist);
		list_add_tail(&tfu->fulist, &tfs->fulist);

		/* If leading digit, repetition count. */
		if (isdigit(*p)) {
			p = next_number(p, &tfu->reps);
			if (!p || (!isspace(*p) && *p != '/'))
				badfmt(fmt);

			/* may overwrite either white space or slash */
			tfu->flags = F_SETREP;
			/* skip trailing white space */
			p = skip_space(++p);
		}

		/* Skip slash and trailing white space. */
		if (*p == '/')
			p = skip_space(++p);

		/* byte count */
		if (isdigit(*p)) {
			p = next_number(p, &tfu->bcnt);
			if (!p || !isspace(*p))
				badfmt(fmt);
			/* skip trailing white space */
			p = skip_space(++p);
		}

		/* format */
		if (*p != '"')
			badfmt(fmt);
		savep = ++p;
		while (*p != '"') {
			if (!*p++)
				badfmt(fmt);
		}
		tfu->fmt = xmalloc(p - savep + 1);
		xstrncpy(tfu->fmt, savep, p - savep + 1);
		escape(tfu->fmt);
		++p;
	}
}

static const char *spec = ".#-+ 0123456789";

int block_size(struct hexdump_fs *fs)
{
	struct hexdump_fu *fu;
	int bcnt, prec, cursize = 0;
	char *fmt;
	struct list_head *p;

	/* figure out the data block size needed for each format unit */
	list_for_each (p, &fs->fulist) {
		fu = list_entry(p, struct hexdump_fu, fulist);
		if (fu->bcnt) {
			cursize += fu->bcnt * fu->reps;
			continue;
		}
		bcnt = prec = 0;
		fmt = fu->fmt;
		while (*fmt) {
			if (*fmt != '%') {
				++fmt;
				continue;
			}
			/*
			 * skip any special chars -- save precision in
			 * case it's a %s format.
			 */
			while (strchr(spec + 1, *++fmt) && *fmt != '\0')
				;
			if (*fmt == '.' && isdigit(*++fmt))
				fmt = next_number(fmt, &prec);
			if (*fmt == '\0')
				badfmt(fu->fmt);
			if (first_letter(fmt, "diouxX"))
				bcnt += 4;
			else if (first_letter(fmt, "efgEG"))
				bcnt += 8;
			else if (*fmt == 's')
				bcnt += prec;
			else if (*fmt == 'c' || (*fmt == '_' && first_letter(++fmt, "cpu")))
				++bcnt;
			++fmt;
		}
		cursize += bcnt * fu->reps;
	}
	return(cursize);
}

void rewrite_rules(struct hexdump_fs *fs, struct hexdump *hex)
{
	enum { NOTOKAY, USEBCNT, USEPREC } sokay;
	struct hexdump_pr *pr;
	struct hexdump_fu *fu;
	struct list_head *p, *q;
	char *p1, *p2, *fmtp;
	char savech, cs[4];
	int nconv, prec = 0;

	list_for_each (p, &fs->fulist) {
		fu = list_entry(p, struct hexdump_fu, fulist);
		/*
		 * Break each format unit into print units; each
		 * conversion character gets its own.
		 */
		nconv = 0;
		fmtp = fu->fmt;
		while (*fmtp) {
			pr = xcalloc(1, sizeof(struct hexdump_pr));
			INIT_LIST_HEAD(&pr->prlist);
			list_add_tail(&pr->prlist, &fu->prlist);

			/* Skip preceding text and up to the next % sign. */
			p1 = fmtp;
			while (*p1 && *p1 != '%')
				++p1;

			/* Only text in the string. */
			if (!*p1) {
				pr->fmt = xstrdup(fmtp);
				pr->flags = F_TEXT;
				break;
			}

			/*
			 * Get precision for %s -- if have a byte count, don't
			 * need it.
			 */
			if (fu->bcnt) {
				sokay = USEBCNT;
				/* skip to conversion character */
				for (p1++; strchr(spec, *p1); p1++)
					;
			} else {
				/* skip any special chars, field width */
				while (strchr(spec + 1, *++p1))
					;
				if (*p1 == '.' && isdigit(*++p1)) {
					sokay = USEPREC;
					p1 = next_number(p1, &prec);
				} else
					sokay = NOTOKAY;
			}

			p2 = p1 + 1;		/* Set end pointer. */
			cs[0] = *p1;		/* Set conversion string. */
			cs[1] = 0;

			/*
			 * Figure out the byte count for each conversion;
			 * rewrite the format as necessary, set up blank-
			 * padding for end of data.
			 */
			if (*cs == 'c') {
				pr->flags = F_CHAR;
				switch(fu->bcnt) {
					case 0:
					case 1:
						pr->bcnt = 1;
						break;
					default:
						p1[1] = '\0';
						badcnt(p1);
				}
			} else if (first_letter(cs, "di")) {
				pr->flags = F_INT;
				goto isint;
			} else if (first_letter(cs, "ouxX")) {
				pr->flags = F_UINT;
isint:				cs[3] = '\0';
				cs[2] = cs[0];
				cs[1] = 'l';
				cs[0] = 'l';
				switch(fu->bcnt) {
					case 0:
						pr->bcnt = 4;
						break;
					case 1:
					case 2:
					case 4:
					case 8:
						pr->bcnt = fu->bcnt;
						break;
					default:
						p1[1] = '\0';
						badcnt(p1);
				}
			} else if (first_letter(cs, "efgEG")) {
				pr->flags = F_DBL;
				switch(fu->bcnt) {
					case 0:
						pr->bcnt = 8;
						break;
					case 4:
					case 8:
						pr->bcnt = fu->bcnt;
						break;
					default:
						p1[1] = '\0';
						badcnt(p1);
				}
			} else if(*cs == 's') {
				pr->flags = F_STR;
				switch(sokay) {
					case NOTOKAY:
						badsfmt();
					case USEBCNT:
						pr->bcnt = fu->bcnt;
						break;
					case USEPREC:
						pr->bcnt = prec;
						break;
				}
			} else if (*cs == '_') {
				++p2;
				switch(p1[1]) {
					case 'A':
						endfu = fu;
						fu->flags |= F_IGNORE;
						/* fallthrough */
					case 'a':
						pr->flags = F_ADDRESS;
						++p2;
						if (first_letter(p1 + 2, "dox")) {
							cs[0] = 'l';
							cs[1] = 'l';
							cs[2] = p1[2];
							cs[3] = '\0';
						} else {
							p1[3] = '\0';
							badconv(p1);
						}
						break;
					case 'c':
						pr->flags = F_C;
						/* cs[0] = 'c';	set in conv_c */
						goto isint2;
					case 'p':
						pr->flags = F_P;
						cs[0] = 'c';
						goto isint2;
					case 'u':
						pr->flags = F_U;
						/* cs[0] = 'c';	set in conv_u */
	isint2:					switch(fu->bcnt) {
							case 0:
							case 1:
								pr->bcnt = 1;
								break;
							default:
								p1[2] = '\0';
								badcnt(p1);
						}
						break;
					default:
						p1[2] = '\0';
						badconv(p1);
				}
			} else {
				p1[1] = '\0';
				badconv(p1);
			}

			/* Color unit(s) specified */
			if (*p2 == '_' && p2[1] == 'L') {
				if (colors_wanted()) {
					char *a;

					/* "cut out" the color_unit(s) */
					a = strchr(p2, '[');
					p2 = strrchr(p2, ']');
					if (a++ && p2)
						pr->colorlist = color_fmt(xstrndup(a, p2++ - a), pr->bcnt);
					else
						badconv(p2);
				}
				/* we don't want colors, quietly skip over them */
				else {
					p2 = strrchr(p2, ']');
					/* be a bit louder if we don't know how to skip over them */
					if (!p2)
						badconv("_L");
					++p2;
				}
			}
			/*
			 * Copy to hexdump_pr format string, set conversion character
			 * pointer, update original.
			 */
			savech = *p2;
			p1[0] = '\0';
			xasprintf(&pr->fmt, "%s%s", fmtp, cs);
			*p2 = savech;
			pr->cchar = pr->fmt + (p1 - fmtp);
			fmtp = p2;

			/* Only one conversion character if byte count */
			if (!(pr->flags&F_ADDRESS) && fu->bcnt && nconv++)
				errx(EXIT_FAILURE,
				    _("byte count with multiple conversion characters"));
		}
		/*
		 * If format unit byte count not specified, figure it out
		 * so can adjust rep count later.
		 */
		if (!fu->bcnt)
			list_for_each(q, &fu->prlist)
				fu->bcnt
				  += (list_entry(q, struct hexdump_pr, prlist))->bcnt;
	}
	/*
	 * If the format string interprets any data at all, and it's
	 * not the same as the blocksize, and its last format unit
	 * interprets any data at all, and has no iteration count,
	 * repeat it as necessary.
	 *
	 * If rep count is greater than 1, no trailing whitespace
	 * gets output from the last iteration of the format unit.
	 */
	list_for_each (p, &fs->fulist) {
		fu = list_entry(p, struct hexdump_fu, fulist);

		if (list_entry_is_last(&fu->fulist, &fs->fulist) &&
			fs->bcnt < hex->blocksize &&
			!(fu->flags&F_SETREP) && fu->bcnt)
				fu->reps += (hex->blocksize - fs->bcnt) / fu->bcnt;
		if (fu->reps > 1 && !list_empty(&fu->prlist)) {
			pr = list_last_entry(&fu->prlist, struct hexdump_pr, prlist);
			if (!pr)
				continue;
			for (p1 = pr->fmt, p2 = NULL; *p1; ++p1)
				p2 = isspace(*p1) ? p1 : NULL;
			if (p2)
				pr->nospace = p2;
		}
	}
}

/* [!]color[:string|:hex_number|:oct_number][@offt|@offt_start-offt_end],... */
static struct list_head *color_fmt(char *cfmt, int bcnt)
{
	struct hexdump_clr *hc, *hcnext;
	struct list_head *ret_head;
	char *clr, *fmt;

	ret_head = xmalloc(sizeof(struct list_head));
	hcnext = hc = xcalloc(1, sizeof(struct hexdump_clr));

	INIT_LIST_HEAD(&hc->colorlist);
	INIT_LIST_HEAD(ret_head);
	list_add_tail(&hc->colorlist, ret_head);

	fmt = cfmt;
	while (cfmt && *cfmt) {
		char *end;
		/* invert this condition */
		if (*cfmt == '!') {
			hcnext->invert = 1;
			++cfmt;
		}

		clr = xstrndup(cfmt, strcspn(cfmt, ":@,"));
		cfmt += strlen(clr);
		hcnext->fmt = color_sequence_from_colorname(clr);
		free(clr);

		if (!hcnext->fmt)
			return NULL;

		/* only colorize this specific value */
		if (*cfmt == ':') {
			++cfmt;
			/* a hex or oct value */
			if (*cfmt == '0') {
				/* hex */
				errno = 0;
				end = NULL;
				if (cfmt[1] == 'x' || cfmt[1] == 'X')
					hcnext->val = strtoul(cfmt + 2, &end, 16);
				else
					hcnext->val = strtoul(cfmt, &end, 8);
				if (errno || end == cfmt)
					badfmt(fmt);
				cfmt = end;
			/* a string */
			} else {
				off_t fmt_end;
				char endchar;
				char *endstr;

				hcnext->val = -1;
				/* temporarily null-delimit the format, so we can reverse-search
				 * for the start of an offset specifier */
				fmt_end = strcspn(cfmt, ",");
				endchar = cfmt[fmt_end];
				cfmt[fmt_end] = '\0';
				endstr = strrchr(cfmt, '@');

				if (endstr) {
					if (endstr[1] != '\0')
						--endstr;
					hcnext->str = xstrndup(cfmt, endstr - cfmt + 1);
				} else
					hcnext->str = xstrndup(cfmt, fmt_end);

				/* restore the character */
				cfmt[fmt_end] = endchar;
				cfmt += strlen(hcnext->str);
			}

		/* no specific value */
		} else
			hcnext->val = -1;

		/* only colorize at this offset */
		hcnext->range = bcnt;
		if (cfmt && *cfmt == '@') {
			errno = 0;
			hcnext->offt = strtoul(++cfmt, &cfmt, 10);
			if (errno)
				badfmt(fmt);

			/* offset range */
			if (*cfmt == '-') {
				++cfmt;
				errno = 0;

				hcnext->range =
				  strtoul(cfmt, &cfmt, 10) - hcnext->offt + 1;
				if (errno)
					badfmt(fmt);
				/* offset range must be between 0 and format byte count */
				if (hcnext->range < 0)
					badcnt("_L");
				/* the offset extends over several print units, clone
				 * the condition, link it in and adjust the address/offset */
				while (hcnext->range > bcnt) {
					hc = xcalloc(1, sizeof(struct hexdump_clr));
					memcpy(hc, hcnext, sizeof(struct hexdump_clr));

					hc->range = bcnt;

					INIT_LIST_HEAD(&hc->colorlist);
					list_add_tail(&hc->colorlist, ret_head);

					hcnext->offt += bcnt;
					hcnext->range -= bcnt;
				}
			}
		/* no specific offset */
		} else
			hcnext->offt = (off_t)-1;

		/* check if the string we're looking for is the same length as the range */
		if (hcnext->str && (int)strlen(hcnext->str) != hcnext->range)
			badcnt("_L");

		/* link in another condition */
		if (cfmt && *cfmt == ',') {
			++cfmt;

			hcnext = xcalloc(1, sizeof(struct hexdump_clr));
			INIT_LIST_HEAD(&hcnext->colorlist);
			list_add_tail(&hcnext->colorlist, ret_head);
		}
	}
	return ret_head;
}

static void escape(char *p1)
{
	char *p2;

	/* alphabetic escape sequences have to be done in place */
	p2 = p1;
	while (TRUE) {
		if (!*p1) {
			*p2 = *p1;
			break;
		}
		if (*p1 == '\\')
			switch(*++p1) {
			case 'a':
			     /* *p2 = '\a'; */
				*p2 = '\007';
				break;
			case 'b':
				*p2 = '\b';
				break;
			case 'f':
				*p2 = '\f';
				break;
			case 'n':
				*p2 = '\n';
				break;
			case 'r':
				*p2 = '\r';
				break;
			case 't':
				*p2 = '\t';
				break;
			case 'v':
				*p2 = '\v';
				break;
			default:
				*p2 = *p1;
				break;
			}
		++p1; ++p2;
	}
}
