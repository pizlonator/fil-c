/* vi: set sw=4 ts=4: */
/*
 * Support code for the hexdump and od applets,
 * based on code from util-linux v 2.11l
 *
 * Copyright (c) 1989
 * The Regents of the University of California.  All rights reserved.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * Original copyright notice is retained at the end of this file.
 */
#include "libbb.h"
#include "dump.h"

#define	F_IGNORE	0x01		/* %_A */
#define	F_SETREP	0x02		/* rep count set, not default */
#define	F_ADDRESS	0x001		/* print offset */
#define	F_BPAD		0x002		/* blank pad */
#define	F_C		0x004		/* %_c */
#define	F_CHAR		0x008		/* %c */
#define	F_DBL		0x010		/* %[EefGf] */
#define	F_INT		0x020		/* %[di] */
#define	F_P		0x040		/* %_p */
#define	F_STR		0x080		/* %s */
#define	F_U		0x100		/* %_u */
#define	F_UINT		0x200		/* %[ouXx] */
#define	F_TEXT		0x400		/* no conversions */

typedef struct priv_dumper_t {
	dumper_t pub;

	char **argv;
	FU *endfu;
	off_t savaddress;        /* saved address/offset in stream */
	off_t eaddress;          /* end address */
	int blocksize;
	smallint exitval;        /* final exit value */

	/* former statics */
	smallint next__done;
	smallint get__ateof; // = 1;
	unsigned char *get__curp;
	unsigned char *get__savp;
} priv_dumper_t;

static const char dot_flags_width_chars[] ALIGN1 = ".#-+ 0123456789";

static const char size_conv_str[] ALIGN1 =
"\x1\x4\x4\x4\x4\x4\x4\x8\x8\x8\x8\x8""cdiouxXeEfgG";
/* c  d  i  o  u  x  X  e  E  f  g  G - bytes contain 'bcnt' for the type */
#define SCS_OFS 12
#define float_convs (size_conv_str + SCS_OFS + sizeof("cdiouxX")-1)
static const char int_convs[] ALIGN1 = "diouxX";

dumper_t* FAST_FUNC alloc_dumper(void)
{
	priv_dumper_t *dumper = xzalloc(sizeof(*dumper));
	dumper->pub.dump_length = -1;
	dumper->pub.dump_vflag = FIRST;
	dumper->get__ateof = 1;
	return &dumper->pub;
}

static NOINLINE int bb_dump_size(FS *fs)
{
	FU *fu;
	int bcnt, cur_size;
	char *fmt;
	const char *p;
	int prec;

	/* figure out the data block size needed for each format unit */
	for (cur_size = 0, fu = fs->nextfu; fu; fu = fu->nextfu) {
		if (fu->bcnt) {
			cur_size += fu->bcnt * fu->reps;
			continue;
		}
		for (bcnt = prec = 0, fmt = fu->fmt; *fmt; ++fmt) {
			if (*fmt != '%')
				continue;
			/*
			 * skip any special chars -- save precision in
			 * case it's a %s format.
			 */
			while (strchr(dot_flags_width_chars + 1, *++fmt))
				continue;
			if (*fmt == '.' && isdigit(*++fmt)) {
				prec = atoi(fmt);
				while (isdigit(*++fmt))
					continue;
			}
			p = strchr(size_conv_str + SCS_OFS, *fmt);
			if (!p) {
				if (*fmt == 's') {
					bcnt += prec;
				}
				if (*fmt == '_') {
					++fmt;
					if ((*fmt == 'c') || (*fmt == 'p') || (*fmt == 'u')) {
						bcnt += 1;
					}
				}
			} else {
				bcnt += p[-SCS_OFS];
			}
		}
		cur_size += bcnt * fu->reps;
	}
	return cur_size;
}

static NOINLINE void rewrite(priv_dumper_t *dumper, FS *fs)
{
	FU *fu;

	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		PR *pr;
		char *p1, *p2, *p3;
		char *fmtp;
		int nconv = 0;
		/*
		 * break each format unit into print units; each
		 * conversion character gets its own.
		 */
		for (fmtp = fu->fmt; *fmtp; ) {
			unsigned len;
			const char *prec;
			const char *byte_count_str;

			/* DBU:[dvae@cray.com] zalloc so that forward ptrs start out NULL */
			pr = xzalloc(sizeof(*pr));
			if (!fu->nextpr)
				fu->nextpr = pr;

			/* skip preceding text and up to the next % sign */
			p1 = strchr(fmtp, '%');
			if (!p1) { /* only text in the string */
				pr->fmt = fmtp;
				pr->flags = F_TEXT;
				break;
			}

			/*
			 * get precision for %s -- if have a byte count, don't
			 * need it.
			 */
			prec = NULL;
			if (fu->bcnt) {
				/* skip to conversion character */
				while (strchr(dot_flags_width_chars, *++p1))
					continue;
			} else {
				/* skip any special chars, field width */
				while (strchr(dot_flags_width_chars + 1, *++p1))
					continue;
				if (*p1 == '.' && isdigit(*++p1)) {
					prec = p1;
					while (isdigit(*++p1))
						continue;
				}
			}

			p2 = p1 + 1; /* set end pointer */

			/*
			 * figure out the byte count for each conversion;
			 * rewrite the format as necessary, set up blank-
			 * padding for end of data.
			 */
			if (*p1 == 'c') {
				pr->flags = F_CHAR;
 DO_BYTE_COUNT_1:
				byte_count_str = "\001";
 DO_BYTE_COUNT:
				if (fu->bcnt) {
					for (;;) {
						if (fu->bcnt == *byte_count_str)
							break;
						if (*++byte_count_str == 0)
							bb_error_msg_and_die("bad byte count for conversion character %s", p1);
					}
				}
				/* Unlike the original, output the remainder of the format string. */
				pr->bcnt = *byte_count_str;
			} else
			if (*p1 == 'l') { /* %ld etc */
				const char *e;

				++p2;
				++p1;
				if (*p1 == 'l') { /* %lld etc */
					++p2;
					++p1;
				}
 DO_INT_CONV:
				e = strchr(int_convs, *p1); /* "diouxX"? */
				if (!e)
					goto DO_BAD_CONV_CHAR;
				pr->flags = F_INT;
				if (e > int_convs + 1) /* not d or i? */
					pr->flags = F_UINT;
				byte_count_str = "\010\004\002\001";
				goto DO_BYTE_COUNT;
			} else
			if (strchr(int_convs, *p1)) { /* %d etc */
				goto DO_INT_CONV;
			} else
			if (strchr(float_convs, *p1)) { /* floating point */
				pr->flags = F_DBL;
				byte_count_str = "\010\004";
				goto DO_BYTE_COUNT;
			} else
			if (*p1 == 's') {
				pr->flags = F_STR;
				pr->bcnt = fu->bcnt;
				if (fu->bcnt == 0) {
					if (!prec)
						bb_simple_error_msg_and_die("%s needs precision or byte count");
					pr->bcnt = atoi(prec);
				}
			} else
			if (*p1 == '_') {
				p2++;  /* move past a in "%_a" */
				switch (p1[1]) {
				case 'A':	/* %_A[dox]: print address and the end */
					dumper->endfu = fu;
					fu->flags |= F_IGNORE;
					/* FALLTHROUGH */
				case 'a':	/* %_a[dox]: current address */
					pr->flags = F_ADDRESS;
					p2++;  /* move past x in "%_ax" */
					if ((p1[2] != 'd') && (p1[2] != 'o') && (p1[2] != 'x')) {
						goto DO_BAD_CONV_CHAR;
					}
					*p1++ = 'l';
					*p1++ = 'l';
					break;
				case 'c':	/* %_c: chars, \ooo, \n \r \t etc */
					pr->flags = F_C;
					/* *p1 = 'c';   set in conv_c */
					goto DO_BYTE_COUNT_1;
				case 'p':	/* %_p: chars, dots for nonprintable */
					pr->flags = F_P;
					*p1 = 'c';
					goto DO_BYTE_COUNT_1;
				case 'u':	/* %_u: chars, 'nul', 'esc' etc for nonprintable */
					pr->flags = F_U;
					/* *p1 = 'c';   set in conv_u */
					goto DO_BYTE_COUNT_1;
				default:
					goto DO_BAD_CONV_CHAR;
				}
			} else {
 DO_BAD_CONV_CHAR:
				bb_error_msg_and_die("bad conversion character %%%s", p1);
			}

			/*
			 * copy to PR format string, set conversion character
			 * pointer, update original.
			 */
			len = (p1 - fmtp) + 1;
			pr->fmt = xstrndup(fmtp, len);
			/* DBU:[dave@cray.com] w/o this, trailing fmt text, space is lost.
			 * Skip subsequent text and up to the next % sign and tack the
			 * additional text onto fmt: eg. if fmt is "%x is a HEX number",
			 * we lose the " is a HEX number" part of fmt.
			 */
			for (p3 = p2; *p3 && *p3 != '%'; p3++)
				continue;
			if ((p3 - p2) != 0) {
				char *d;
				pr->fmt = d = xrealloc(pr->fmt, len + (p3 - p2) + 1);
				d += len;
				do {
					*d++ = *p2++;
				} while (p2 != p3);
				*d = '\0';
				/* now p2 = p3 */
			}
			pr->cchar = pr->fmt + len - 1; /* must be after realloc! */
			fmtp = p2;

			/* only one conversion character if byte count */
			if (!(pr->flags & F_ADDRESS) && fu->bcnt && nconv++) {
				bb_simple_error_msg_and_die("byte count with multiple conversion characters");
			}
		}
		/*
		 * if format unit byte count not specified, figure it out
		 * so can adjust rep count later.
		 */
		if (fu->bcnt == 0)
			for (pr = fu->nextpr; pr; pr = pr->nextpr)
				fu->bcnt += pr->bcnt;
	}
	/*
	 * if the format string interprets any data at all, and it's
	 * not the same as the blocksize, and its last format unit
	 * interprets any data at all, and has no iteration count,
	 * repeat it as necessary.
	 *
	 * if rep count is greater than 1, no trailing whitespace
	 * gets output from the last iteration of the format unit:
	 * 2/1 "%02x " prints "XX XX", not "XX XX "
	 * 2/1 "%02x\n" prints "XX\nXX", not "XX\nXX\n"
	 */
	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		if (!fu->nextfu
		 && fs->bcnt < dumper->blocksize
		 && !(fu->flags & F_SETREP)
		 && fu->bcnt
		) {
			fu->reps += (dumper->blocksize - fs->bcnt) / fu->bcnt;
		}
		if (fu->reps > 1 && fu->nextpr) {
			PR *pr;
			char *p1, *p2;

			for (pr = fu->nextpr;; pr = pr->nextpr)
				if (!pr->nextpr)
					break;
			p2 = NULL;
			for (p1 = pr->fmt; *p1; ++p1)
				p2 = isspace(*p1) ? p1 : NULL;
			pr->nospace = p2;
		}
	}
}

static void do_skip(priv_dumper_t *dumper, const char *fname)
{
	struct stat sbuf;

	xfstat(STDIN_FILENO, &sbuf, fname);
	if (S_ISREG(sbuf.st_mode)
	 && dumper->pub.dump_skip >= sbuf.st_size
	) {
		/* If st_size is valid and pub.dump_skip >= st_size */
		dumper->pub.dump_skip -= sbuf.st_size;
		dumper->pub.address += sbuf.st_size;
		return;
	}
	if (fseeko(stdin, dumper->pub.dump_skip, SEEK_SET)) {
		bb_simple_perror_msg_and_die(fname);
	}
	dumper->pub.address += dumper->pub.dump_skip;
	dumper->savaddress = dumper->pub.address;
	dumper->pub.dump_skip = 0;
}

static NOINLINE int next(priv_dumper_t *dumper)
{
	for (;;) {
		const char *fname = *dumper->argv;

		if (fname) {
			dumper->argv++;
			if (NOT_LONE_DASH(fname)) {
				if (!freopen(fname, "r", stdin)) {
					bb_simple_perror_msg(fname);
					dumper->exitval = 1;
					dumper->next__done = 1;
					continue;
				}
			}
		} else {
			if (dumper->next__done)
				return 0; /* no next file */
		}
		dumper->next__done = 1;
		if (dumper->pub.dump_skip)
			do_skip(dumper, fname ? fname : "stdin");
		if (dumper->pub.dump_skip == 0)
			return 1;
	}
	/* NOTREACHED */
}

static unsigned char *get(priv_dumper_t *dumper)
{
	int n;
	int need, nread;
	int blocksize = dumper->blocksize;

	if (!dumper->get__curp) {
		dumper->pub.address = (off_t)0; /*DBU:[dave@cray.com] initialize,initialize..*/
		dumper->get__curp = xmalloc(blocksize);
		dumper->get__savp = xzalloc(blocksize); /* need to be initialized */
	} else {
		unsigned char *tmp = dumper->get__curp;
		dumper->get__curp = dumper->get__savp;
		dumper->get__savp = tmp;
		dumper->savaddress += blocksize;
		dumper->pub.address = dumper->savaddress;
	}
	need = blocksize;
	nread = 0;
	while (1) {
		/*
		 * if read the right number of bytes, or at EOF for one file,
		 * and no other files are available, zero-pad the rest of the
		 * block and set the end flag.
		 */
		if (!dumper->pub.dump_length || (dumper->get__ateof && !next(dumper))) {
			if (need == blocksize) {
				return NULL;
			}
			if (dumper->pub.dump_vflag != ALL   /* not "show all"? */
			 && dumper->pub.dump_vflag != FIRST /* not first line? */
			 && memcmp(dumper->get__curp, dumper->get__savp, nread) == 0 /* same data? */
			) {
				if (dumper->pub.dump_vflag != DUP) {
					puts("*");
				}
			}
			memset(dumper->get__curp + nread, 0, need);
			dumper->eaddress = dumper->pub.address + nread;
			return dumper->get__curp;
		}
		n = fread(dumper->get__curp + nread, sizeof(unsigned char),
				dumper->pub.dump_length == -1 ? need : MIN(dumper->pub.dump_length, need), stdin);
		if (n == 0) {
			if (ferror(stdin)) {
				bb_simple_perror_msg(dumper->argv[-1]);
			}
			dumper->get__ateof = 1;
			continue;
		}
		dumper->get__ateof = 0;
		if (dumper->pub.dump_length != -1) {
			dumper->pub.dump_length -= n;
		}
		need -= n;
		if (need == 0) {
			if (dumper->pub.dump_vflag == ALL   /* "show all"? */
			 || dumper->pub.dump_vflag == FIRST /* first line? */
			 || memcmp(dumper->get__curp, dumper->get__savp, blocksize) != 0 /* not same data? */
			) {
				if (dumper->pub.dump_vflag == DUP || dumper->pub.dump_vflag == FIRST) {
					dumper->pub.dump_vflag = WAIT;
				}
				return dumper->get__curp;
			}
			if (dumper->pub.dump_vflag == WAIT) {
				puts("*");
			}
			dumper->pub.dump_vflag = DUP;
			dumper->savaddress += blocksize;
			dumper->pub.address = dumper->savaddress;
			need = blocksize;
			nread = 0;
		} else {
			nread += n;
		}
	}
}

static void bpad(PR *pr)
{
	char *p1, *p2;

	/*
	 * remove all conversion flags; '-' is the only one valid
	 * with %s, and it's not useful here.
	 */
	pr->flags = F_BPAD;
	*pr->cchar = 's';
	for (p1 = pr->fmt; *p1 != '%'; ++p1)
		continue;
	for (p2 = ++p1; *p1 && strchr(" -0+#", *p1); ++p1)
		if (pr->nospace)
			pr->nospace--;
	while ((*p2++ = *p1++) != '\0')
		continue;
}

static const char conv_str[] ALIGN1 =
	"\0"  "\\""0""\0"
	"\007""\\""a""\0"
	"\b"  "\\""b""\0"
	"\f"  "\\""f""\0"
	"\n"  "\\""n""\0"
	"\r"  "\\""r""\0"
	"\t"  "\\""t""\0"
	"\v"  "\\""v""\0"
	;

static void conv_c(PR *pr, unsigned char *p)
{
	const char *str = conv_str;

	do {
		if (*p == *str) {
			++str;
			goto strpr; /* map e.g. '\n' to "\\n" */
		}
		str += 4;
	} while (*str);

	if (isprint_asciionly(*p)) {
		*pr->cchar = 'c';
		printf(pr->fmt, *p);
	} else {
		char buf[4];
		/* gcc-8.0.1 needs lots of casts to shut up */
		sprintf(buf, "%03o", (unsigned)(uint8_t)*p);
		str = buf;
 strpr:
		*pr->cchar = 's';
		printf(pr->fmt, str);
	}
}

static void conv_u(PR *pr, unsigned char *p)
{
	static const char list[] ALIGN1 =
		"nul\0soh\0stx\0etx\0eot\0enq\0ack\0bel\0"
		"bs\0_ht\0_lf\0_vt\0_ff\0_cr\0_so\0_si\0_"
		"dle\0dc1\0dc2\0dc3\0dc4\0nak\0syn\0etb\0"
		"can\0em\0_sub\0esc\0fs\0_gs\0_rs\0_us";
	/* NB: bug: od uses %_u to implement -a,
	 * but it should use "nl", not "lf", for char #10.
	 */

	if (*p <= 0x1f) {
		*pr->cchar = 's';
		printf(pr->fmt, list + (4 * (int)*p));
	} else if (*p == 0x7f) {
		*pr->cchar = 's';
		printf(pr->fmt, "del");
	} else if (*p < 0x7f) { /* isprint() */
		*pr->cchar = 'c';
		printf(pr->fmt, *p);
	} else {
		*pr->cchar = 'x';
		printf(pr->fmt, (int) *p);
	}
}

static NOINLINE void display(priv_dumper_t* dumper)
{
	unsigned char *bp;

	while ((bp = get(dumper)) != NULL) {
		FS *fs;
		unsigned char *savebp;
		off_t saveaddress;

		fs = dumper->pub.fshead;
		savebp = bp;
		saveaddress = dumper->pub.address;
		for (; fs; fs = fs->nextfs, bp = savebp, dumper->pub.address = saveaddress) {
			FU *fu;
			for (fu = fs->nextfu; fu; fu = fu->nextfu) {
				int cnt;
				if (fu->flags & F_IGNORE) {
					break;
				}
				for (cnt = fu->reps; cnt; --cnt) {
					PR *pr;
					for (pr = fu->nextpr; pr; dumper->pub.address += pr->bcnt,
								bp += pr->bcnt, pr = pr->nextpr) {
						unsigned char savech;

						if (dumper->eaddress
						 && dumper->pub.address >= dumper->eaddress
						) {
#if ENABLE_XXD
							if (dumper->pub.xxd_eofstring) {
								/* xxd support: requested to not pad incomplete blocks */
								fputs_stdout(dumper->pub.xxd_eofstring);
								return;
							}
#endif
#if ENABLE_OD
							if (dumper->pub.od_eofstring) {
								/* od support: requested to not pad incomplete blocks */
								/* ... but do print final offset */
								fputs_stdout(dumper->pub.od_eofstring);
								goto endfu;
							}
#endif
							if (!(pr->flags & (F_TEXT | F_BPAD)))
								bpad(pr);
						}
						savech = '\0';
						if (cnt == 1 && pr->nospace) {
							savech = *pr->nospace;
							*pr->nospace = '\0';
						}
						switch (pr->flags) {
						case F_ADDRESS:
							printf(pr->fmt, (unsigned long long) dumper->pub.address
#if ENABLE_XXD
								+ dumper->pub.xxd_displayoff
#endif
							);
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
						case F_DBL: {
							double dval;
							float fval;

							switch (pr->bcnt) {
							case 4:
								memcpy(&fval, bp, sizeof(fval));
								printf(pr->fmt, fval);
								break;
							case 8:
								memcpy(&dval, bp, sizeof(dval));
								printf(pr->fmt, dval);
								break;
							}
							break;
						}
						case F_INT: {
							union {
								int16_t ival16;
								int32_t ival32;
								int64_t ival64;
							} u;
							int value = (signed char)*bp;

							switch (pr->bcnt) {
							case 1:
								break;
							case 2:
								move_from_unaligned16(u.ival16, bp);
								value = u.ival16;
								break;
							case 4:
								move_from_unaligned32(u.ival32, bp);
								value = u.ival32;
								break;
							case 8:
								move_from_unaligned64(u.ival64, bp);
//A hack. Users _must_ use %llX formats to not truncate high bits
								printf(pr->fmt, (long long)u.ival64);
								goto skip;
							}
							printf(pr->fmt, value);
 skip:
							break;
						}
						case F_P:
							printf(pr->fmt, isprint_asciionly(*bp) ? *bp : '.');
							break;
						case F_STR:
							printf(pr->fmt, (char *) bp);
							break;
						case F_TEXT:
							fputs_stdout(pr->fmt);
							break;
						case F_U:
							conv_u(pr, bp);
							break;
						case F_UINT: {
							unsigned value = (unsigned char)*bp;
							switch (pr->bcnt) {
							case 1:
								break;
							case 2:
								move_from_unaligned16(value, bp);
								break;
							case 4:
								move_from_unaligned32(value, bp);
								break;
							/* case 8: no users yet */
							}
							printf(pr->fmt, value);
							break;
						}
						}
						if (savech) {
							*pr->nospace = savech;
						}
					}
				}
			}
		}
	}
 IF_OD(endfu:)
	if (dumper->endfu) {
		PR *pr;
		/*
		 * if eaddress not set, error or file size was multiple
		 * of blocksize, and no partial block ever found.
		 */
		if (!dumper->eaddress) {
			if (!dumper->pub.address) {
				return;
			}
			dumper->eaddress = dumper->pub.address;
		}
		for (pr = dumper->endfu->nextpr; pr; pr = pr->nextpr) {
			switch (pr->flags) {
			case F_ADDRESS:
				printf(pr->fmt, (unsigned long long) dumper->eaddress
#if ENABLE_XXD
					+ dumper->pub.xxd_displayoff
#endif
				);
				break;
			case F_TEXT:
				fputs_stdout(pr->fmt);
				break;
			}
		}
	}
}

#define dumper ((priv_dumper_t*)pub_dumper)
int FAST_FUNC bb_dump_dump(dumper_t *pub_dumper, char **argv)
{
	FS *tfs;
	int blocksize;

	/* figure out the data block size */
	blocksize = 0;
	tfs = dumper->pub.fshead;
	while (tfs) {
		tfs->bcnt = bb_dump_size(tfs);
		if (blocksize < tfs->bcnt) {
			blocksize = tfs->bcnt;
		}
		tfs = tfs->nextfs;
	}
	dumper->blocksize = blocksize;

	/* rewrite the rules, do syntax checking */
	for (tfs = dumper->pub.fshead; tfs; tfs = tfs->nextfs) {
		rewrite(dumper, tfs);
	}

	dumper->argv = argv;
	display(dumper);

	return dumper->exitval;
}

void FAST_FUNC bb_dump_add(dumper_t* pub_dumper, const char *fmt)
{
	const char *p;
	FS *tfs;
	FU **nextfupp;

	/* start new linked list of format units */
	tfs = xzalloc(sizeof(FS)); /*DBU:[dave@cray.com] start out NULL */
	if (!dumper->pub.fshead) {
		dumper->pub.fshead = tfs;
	} else {
		FS *fslast = dumper->pub.fshead;
		while (fslast->nextfs)
			fslast = fslast->nextfs;
		fslast->nextfs = tfs;
	}
	nextfupp = &tfs->nextfu;

	/* take the format string and break it up into format units */
	p = fmt;
	for (;;) {
		FU *tfu;
		const char *savep;

		p = skip_whitespace(p);
		if (*p == '\0') {
			break;
		}

		/* allocate a new format unit and link it in */
		/* NOSTRICT */
		/* DBU:[dave@cray.com] zalloc so that forward pointers start out NULL */
		tfu = xzalloc(sizeof(FU));
		*nextfupp = tfu;
		nextfupp = &tfu->nextfu;
		tfu->reps = 1;

		/* if leading digit, repetition count */
		if (isdigit(*p)) {
			for (savep = p; isdigit(*p); ++p)
				continue;
			if (!isspace(*p) && *p != '/') {
				bb_error_msg_and_die("bad format {%s}", fmt);
			}
			/* may overwrite either white space or slash */
			tfu->reps = atoi(savep);
			tfu->flags = F_SETREP;
			/* skip trailing white space */
			p = skip_whitespace(++p);
		}

		/* skip slash and trailing white space */
		if (*p == '/') {
			p = skip_whitespace(p + 1);
		}

		/* byte count */
		if (isdigit(*p)) {
// TODO: use bb_strtou
			savep = p;
			while (isdigit(*++p))
				continue;
			if (!isspace(*p)) {
				bb_error_msg_and_die("bad format {%s}", fmt);
			}
// Above check prohibits formats such as '/1"%02x"' - it requires space after 1.
// Other than this, formats can be pretty much jammed together:
// "%07_ax:"8/2 "%04x|""\n"
// but this space is required. The check *can* be removed, but
// keeping it to stay compat with util-linux hexdump.
			tfu->bcnt = atoi(savep);
			/* skip trailing white space */
			p = skip_whitespace(p + 1);
		}

		/* format */
		if (*p != '"') {
			bb_error_msg_and_die("bad format {%s}", fmt);
		}
		for (savep = ++p; *p != '"';) {
			if (*p++ == '\0') {
				bb_error_msg_and_die("bad format {%s}", fmt);
			}
		}
		tfu->fmt = xstrndup(savep, p - savep);

		/* alphabetic escape sequences have to be done in place */
		strcpy_and_process_escape_sequences(tfu->fmt, tfu->fmt);
		/* unknown mappings are not changed: "\z" -> '\\' 'z' */
		/* trailing backslash, if any, is preserved */
#if 0
		char *p1;
		char *p2;
		p1 = tfu->fmt;
		for (p2 = p1;; ++p1, ++p2) {
			*p2 = *p1;
			if (*p1 == '\0')
				break;

			if (*p1 == '\\') {
				const char *cs;

				p1++;
				*p2 = *p1;
				if (*p1 == '\0') {
					/* "...\" trailing backslash. Eaten. */
					break;
				}
				cs = conv_str + 4; /* skip NUL element */
				do {
					/* map e.g. "\n" -> '\n' */
					if (*p1 == cs[2]) {
						*p2 = cs[0];
						break;
					}
					cs += 4;
				} while (*cs);
				/* unknown mappings remove bkslash: "\z" -> 'z' */
			}
		}
#endif

		p++;
	}
}

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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND
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
