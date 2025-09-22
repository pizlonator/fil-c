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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hexdump.h"
#include "xalloc.h"
#include "c.h"
#include "nls.h"
#include "colors.h"

static void doskip(const char *, int, struct hexdump *);
static u_char *get(struct hexdump *);

enum _vflag vflag = FIRST;

static off_t address;			/* address/offset in stream */
static off_t eaddress;			/* end address */

static const char *color_cond(struct hexdump_pr *pr, unsigned char *bp, int bcnt)
{
	register struct list_head *p;
	register struct hexdump_clr *clr;
	off_t offt;
	int match;

	list_for_each(p, pr->colorlist) {
		clr = list_entry(p, struct hexdump_clr, colorlist);
		offt = clr->offt;
		match = 0;

		/* no offset or offset outside this print unit */
		if (offt < 0)
			offt = address;
		if (offt < address || offt + clr->range > address + bcnt)
			continue;

		/* match a string */
		if (clr->str) {
			if (pr->flags == F_ADDRESS) {
				/* TODO */
			}
			else if (!strncmp(clr->str, (char *)bp + offt
				- address, clr->range))
				match = 1;
		/* match a value */
		} else if (clr->val != -1) {
			int val = 0;
			/* addresses are not part of the input, so we can't
			 * compare with the contents of bp */
			if (pr->flags == F_ADDRESS) {
				if (clr->val == address)
					match = 1;
			} else {
				memcpy(&val, bp + offt - address, clr->range);
				if (val == clr->val)
					match = 1;
			}
		/* no conditions, only a color was specified */
		} else
			return clr->fmt;

		/* return the format string or check for another */
		if (match ^ clr->invert)
			return clr->fmt;
	}

	/* no match */
	return NULL;
}

static inline void
print(struct hexdump_pr *pr, unsigned char *bp) {

	const char *color = NULL;

	if (pr->colorlist && (color = color_cond(pr, bp, pr->bcnt)))
		color_enable(color);

	switch(pr->flags) {
	case F_ADDRESS:
		printf(pr->fmt, address);
		break;
	case F_BPAD:
		printf(pr->fmt, "");
		break;
	case F_C:
		conv_c(pr, bp);
		break;
	case F_CHAR:
		printf(pr->fmt, *bp);
		break;
	case F_DBL:
	    {
		double dval;
		float fval;
		switch(pr->bcnt) {
		case 4:
			memmove(&fval, bp, sizeof(fval));
			printf(pr->fmt, fval);
			break;
		case 8:
			memmove(&dval, bp, sizeof(dval));
			printf(pr->fmt, dval);
			break;
		}
		break;
	    }
	case F_INT:
	    {
		char cval;	/* int8_t */
		short sval;	/* int16_t */
		int ival;	/* int32_t */
		long long Lval;	/* int64_t, int64_t */

		switch(pr->bcnt) {
		case 1:
			memmove(&cval, bp, sizeof(cval));
			printf(pr->fmt, (unsigned long long) cval);
			break;
		case 2:
			memmove(&sval, bp, sizeof(sval));
			printf(pr->fmt, (unsigned long long) sval);
			break;
		case 4:
			memmove(&ival, bp, sizeof(ival));
			printf(pr->fmt, (unsigned long long) ival);
			break;
		case 8:
			memmove(&Lval, bp, sizeof(Lval));
			printf(pr->fmt, Lval);
			break;
		}
		break;
	    }
	case F_P:
		printf(pr->fmt, isprint(*bp) ? *bp : '.');
		break;
	case F_STR:
		printf(pr->fmt, (char *)bp);
		break;
	case F_TEXT:
		printf("%s", pr->fmt);
		break;
	case F_U:
		conv_u(pr, bp);
		break;
	case F_UINT:
	    {
		unsigned short sval;	/* u_int16_t */
		unsigned int ival;	/* u_int32_t */
		unsigned long long Lval;/* u_int64_t, u_int64_t */

		switch(pr->bcnt) {
		case 1:
			printf(pr->fmt, (unsigned long long) *bp);
			break;
		case 2:
			memmove(&sval, bp, sizeof(sval));
			printf(pr->fmt, (unsigned long long) sval);
			break;
		case 4:
			memmove(&ival, bp, sizeof(ival));
			printf(pr->fmt, (unsigned long long) ival);
			break;
		case 8:
			memmove(&Lval, bp, sizeof(Lval));
			printf(pr->fmt, Lval);
			break;
		}
		break;
	    }
	}
	if (color) /* did we colorize something? */
		color_disable();
}

static void bpad(struct hexdump_pr *pr)
{
	static const char *spec = " -0+#";
	char *p1, *p2;

	/*
	 * remove all conversion flags; '-' is the only one valid
	 * with %s, and it's not useful here.
	 */
	pr->flags = F_BPAD;
	pr->cchar[0] = 's';
	pr->cchar[1] = 0;

	p1 = pr->fmt;
	while (*p1 != '%')
		++p1;

	p2 = ++p1;
	while (*p1 && strchr(spec, *p1))
		++p1;

	while ((*p2++ = *p1++))
		;
}

void display(struct hexdump *hex)
{
	register struct list_head *fs;
	register struct hexdump_fs *fss;
	register struct hexdump_fu *fu;
	register struct hexdump_pr *pr;
	register int cnt;
	register unsigned char *bp;
	off_t saveaddress;
	unsigned char savech = 0, *savebp;
	struct list_head *p, *q, *r;

	while ((bp = get(hex)) != NULL) {
		ssize_t rem = hex->blocksize;

		fs = &hex->fshead; savebp = bp; saveaddress = address;

		list_for_each(p, fs) {
			fss = list_entry(p, struct hexdump_fs, fslist);

			list_for_each(q, &fss->fulist) {
				fu = list_entry(q, struct hexdump_fu, fulist);

				if (fu->flags&F_IGNORE)
					break;

				cnt = fu->reps;

				while (cnt && rem >= 0) {
					list_for_each(r, &fu->prlist) {
						pr = list_entry(r, struct hexdump_pr, prlist);

						if (eaddress && address >= eaddress
						    && !(pr->flags&(F_TEXT|F_BPAD)))
							bpad(pr);

						if (cnt == 1 && pr->nospace) {
							savech = *pr->nospace;
							*pr->nospace = '\0';
							print(pr, bp);
							*pr->nospace = savech;
						} else
							print(pr, bp);

						address += pr->bcnt;

						rem -= pr->bcnt;
						if (rem < 0)
							break;

						bp += pr->bcnt;
					}
					--cnt;
				}
			}
			bp = savebp;
			rem = hex->blocksize;
			address = saveaddress;
		}
	}
	if (endfu) {
		/*
		 * if eaddress not set, error or file size was multiple of
		 * blocksize, and no partial block ever found.
		 */
		if (!eaddress) {
			if (!address)
				return;
			eaddress = address;
		}
		list_for_each (p, &endfu->prlist) {
			const char *color = NULL;

			pr = list_entry(p, struct hexdump_pr, prlist);
			if (colors_wanted() && pr->colorlist
			    && (color = color_cond(pr, bp, pr->bcnt))) {
				color_enable(color);
			}

			switch(pr->flags) {
			case F_ADDRESS:
				printf(pr->fmt, eaddress);
				break;
			case F_TEXT:
				printf("%s", pr->fmt);
				break;
			}
			if (color) /* did we highlight something? */
				color_disable();
		}
	}
}

static char **_argv;

static u_char *
get(struct hexdump *hex)
{
	static int ateof = 1;
	static u_char *curp, *savp;
	ssize_t n, need, nread;
	u_char *tmpp;

	if (!curp) {
		curp = xcalloc(1, hex->blocksize);
		savp = xcalloc(1, hex->blocksize);
	} else {
		tmpp = curp;
		curp = savp;
		savp = tmpp;
		address += hex->blocksize;
	}
	need = hex->blocksize, nread = 0;
	while (TRUE) {
		/*
		 * if read the right number of bytes, or at EOF for one file,
		 * and no other files are available, zero-pad the rest of the
		 * block and set the end flag.
		 */
		if (!hex->length || (ateof && !next(NULL, hex))) {
			if (need == hex->blocksize)
				goto retnul;
			if (!need && vflag != ALL &&
			    !memcmp(curp, savp, nread)) {
				if (vflag != DUP)
					printf("*\n");
				goto retnul;
			}
			if (need > 0)
				memset((char *)curp + nread, 0, need);
			eaddress = address + nread;
			return(curp);
		}
		if (fileno(stdin) == -1) {
			warnx(_("all input file arguments failed"));
			goto retnul;
		}
		n = fread((char *)curp + nread, sizeof(unsigned char),
		    hex->length == -1 ? need : min(hex->length, need), stdin);
		if (!n) {
			if (ferror(stdin))
				warn("%s", _argv[-1]);
			ateof = 1;
			continue;
		}
		ateof = 0;
		if (hex->length != -1)
			hex->length -= n;
		if (!(need -= n)) {
			if (vflag == ALL || vflag == FIRST ||
			    memcmp(curp, savp, hex->blocksize) != 0) {
				if (vflag == DUP || vflag == FIRST)
					vflag = WAIT;
				return(curp);
			}
			if (vflag == WAIT)
				printf("*\n");
			vflag = DUP;
			address += hex->blocksize;
			need = hex->blocksize;
			nread = 0;
		}
		else
			nread += n;
	}
retnul:
	free (curp);
	free (savp);
	return NULL;
}

int next(char **argv, struct hexdump *hex)
{
	static int done;
	int statok;

	if (argv) {
		_argv = argv;
		return(1);
	}
	while (TRUE) {
		if (*_argv) {
			if (!(freopen(*_argv, "r", stdin))) {
				warn("%s", *_argv);
				hex->exitval = EXIT_FAILURE;
				++_argv;
				continue;
			}
			statok = done = 1;
		} else {
			if (done++)
				return(0);
			statok = 0;
		}
		if (hex->skip)
			doskip(statok ? *_argv : "stdin", statok, hex);
		if (*_argv)
			++_argv;
		if (!hex->skip)
			return(1);
	}
	/* NOTREACHED */
}

static void
doskip(const char *fname, int statok, struct hexdump *hex)
{
	struct stat sbuf;

	if (statok) {
		if (fstat(fileno(stdin), &sbuf))
		        err(EXIT_FAILURE, "%s", fname);
		if (S_ISREG(sbuf.st_mode) && hex->skip > sbuf.st_size) {
		  /* If size valid and skip >= size */
			hex->skip -= sbuf.st_size;
			address += sbuf.st_size;
			return;
		}
	}
	/* sbuf may be undefined here - do not test it */
	if (fseek(stdin, hex->skip, SEEK_SET))
	        err(EXIT_FAILURE, "%s", fname);
	address += hex->skip;
	hex->skip = 0;
}
