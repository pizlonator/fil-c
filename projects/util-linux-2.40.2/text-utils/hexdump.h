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
 *
 *	@(#)hexdump.h	5.4 (Berkeley) 6/1/90
 */
#ifndef UTIL_LINUX_HEXDUMP_H
#define UTIL_LINUX_HEXDUMP_H

#include "c.h"
#include "list.h"

struct hexdump_clr {
	struct list_head colorlist;	/* next color unit */
	const char *fmt;		/* the color, UL_COLOR_* */
	off_t offt;			/* offset where unit is valid... */
	int range;			/* ... and it's range */
	int val;			/* value ... */
	char *str;			/* ... or string to match */
	int invert;			/* invert condition? */
};

struct hexdump_pr {
	struct list_head prlist;		/* next print unit */
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
	unsigned int flags;		/* flag values */
	int bcnt;			/* byte count */
	char *cchar;			/* conversion character */
	struct list_head *colorlist;	/* color settings */
	char *fmt;			/* printf format */
	char *nospace;			/* no whitespace version */
};

struct hexdump_fu {
	struct list_head fulist;		/* next format unit */
	struct list_head prlist;		/* next print unit */
#define	F_IGNORE	0x01		/* %_A */
#define	F_SETREP	0x02		/* rep count set, not default */
	unsigned int flags;		/* flag values */
	int reps;			/* repetition count */
	int bcnt;			/* byte count */
	char *fmt;			/* format string */
};

struct hexdump_fs {			/* format strings */
	struct list_head fslist;		/* linked list of format strings */
	struct list_head fulist;		/* linked list of format units */
	int bcnt;
};

struct hexdump {
  struct list_head fshead;				/* head of format strings */
  ssize_t blocksize;			/* data block size */
  int exitval;				/* final exit value */
  ssize_t length;			/* max bytes to read */
  off_t skip;				/* bytes to skip */
};

extern struct hexdump_fu *endfu;

enum _vflag { ALL, DUP, FIRST, WAIT };	/* -v values */
extern enum _vflag vflag;

int block_size(struct hexdump_fs *);
void add_fmt(const char *, struct hexdump *);
void rewrite_rules(struct hexdump_fs *, struct hexdump *);
void addfile(char *, struct hexdump *);
void display(struct hexdump *);
void __attribute__((__noreturn__)) usage(void);
void conv_c(struct hexdump_pr *, u_char *);
void conv_u(struct hexdump_pr *, u_char *);
int  next(char **, struct hexdump *);
int parse_args(int, char **, struct hexdump *);

#endif /* UTIL_LINUX_HEXDUMP_H */
