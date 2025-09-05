/* $OpenBSD: cmode.c,v 1.22 2023/04/21 13:39:37 op Exp $ */
/*
 * This file is in the public domain.
 *
 * Author: Kjell Wooding <kjell@openbsd.org>
 */

/*
 * Implement an non-irritating KNF-compliant mode for editing
 * C code.
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static int cc_strip_trailp = TRUE;	/* Delete Trailing space? */
static int cc_basic_indent = 8;		/* Basic Indent multiple */
static int cc_cont_indent = 4;		/* Continued line indent */
static int cc_colon_indent = -8;	/* Label / case indent */

static int getmatch(int, int);
static int getindent(const struct line *, int *);
static int in_whitespace(struct line *, int);
static int findcolpos(const struct buffer *, const struct line *, int);
static struct line *findnonblank(struct line *);
static int isnonblank(const struct line *, int);

void cmode_init(void);
int cc_comment(int, int);

/* Keymaps */

static PF cmode_brace[] = {
	cc_brace,	/* } */
};

#ifdef ENABLE_COMPILE_GREP
static PF cmode_cCP[] = {
	compile,		/* C-c P */
};
#endif

static PF cmode_cc[] = {
	NULL,		/* ^C */
	rescan,		/* ^D */
	rescan,		/* ^E */
	rescan,		/* ^F */
	rescan,		/* ^G */
	rescan,		/* ^H */
	cc_tab,		/* ^I */
	rescan,		/* ^J */
	rescan,		/* ^K */
	rescan,		/* ^L */
	cc_lfindent,	/* ^M */
};

static PF cmode_spec[] = {
	cc_char,	/* : */
};

#ifdef ENABLE_COMPILE_GREP
static struct KEYMAPE (1) cmode_cmap = {
	1,
	1,
	rescan,
	{
		{ 'P', 'P', cmode_cCP, NULL }
	}
};
#endif

static struct KEYMAPE (3) cmodemap = {
	3,
	3,
	rescan,
	{
#ifdef ENABLE_COMPILE_GREP
		{ CCHR('C'), CCHR('M'), cmode_cc, (KEYMAP *) &cmode_cmap },
#endif
		{ ':', ':', cmode_spec, NULL },
		{ '}', '}', cmode_brace, NULL }
	}
};

/* Function, Mode hooks */

void
cmode_init(void)
{
	funmap_add(cmode, "c-mode", 0);
	funmap_add(cc_char, "c-handle-special-char", 0);
	funmap_add(cc_brace, "c-handle-special-brace", 0);
	funmap_add(cc_tab, "c-tab-or-indent", 0);
	funmap_add(cc_indent, "c-indent", 0);
	funmap_add(cc_lfindent, "c-indent-and-newline", 0);
	maps_add((KEYMAP *)&cmodemap, "c");
}

/*
 * Enable/toggle c-mode
 */
int
cmode(int f, int n)
{
	return(changemode(f, n, "c"));
}

/*
 * Handle special C character - selfinsert then indent.
 */
int
cc_char(int f, int n)
{
	if (n < 0)
		return (FALSE);
	if (selfinsert(FFRAND, n) == FALSE)
		return (FALSE);
	return (cc_indent(FFRAND, n));
}

/*
 * Handle special C character - selfinsert then indent.
 */
int
cc_brace(int f, int n)
{
	if (n < 0)
		return (FALSE);
	if (showmatch(FFRAND, 1) == FALSE)
		return (FALSE);
	return (cc_indent(FFRAND, n));
}


/*
 * If we are in the whitespace at the beginning of the line,
 * simply act as a regular tab. If we are not, indent
 * current line according to whitespace rules.
 */
int
cc_tab(int f, int n)
{
	int inwhitep = FALSE;	/* In leading whitespace? */

	inwhitep = in_whitespace(curwp->w_dotp, llength(curwp->w_dotp));

	/* If empty line, or in whitespace */
	if (llength(curwp->w_dotp) == 0 || inwhitep)
		return (selfinsert(f, n));

	return (cc_indent(FFRAND, 1));
}

/*
 * Attempt to indent current line according to KNF rules.
 */
int
cc_indent(int f, int n)
{
	int pi, mi;			/* Previous indents (mi is ignored) */
	int ci;				/* current indent */
	struct line *lp;
	int ret;

	if (n < 0)
		return (FALSE);

	undo_boundary_enable(FFRAND, 0);
	if (cc_strip_trailp)
		deltrailwhite(FFRAND, 1);

	/*
	 * Search backwards for a non-blank, non-preprocessor,
	 * non-comment line
	 */

	lp = findnonblank(curwp->w_dotp);

	pi = getindent(lp, &mi);

	/* Strip leading space on current line */
	delleadwhite(FFRAND, 1);
	/* current indent is computed only to current position */
	(void)getindent(curwp->w_dotp, &ci);

	if (pi + ci < 0)
		ret = indent(FFOTHARG, 0);
	else
		ret = indent(FFOTHARG, pi + ci);

	undo_boundary_enable(FFRAND, 1);

	return (ret);
}

/*
 * Indent-and-newline (technically, newline then indent)
 */
int
cc_lfindent(int f, int n)
{
	if (n < 0)
		return (FALSE);
	if (cc_strip_trailp)
		deltrailwhite(FFRAND, 1);
	if (enewline(FFRAND, 1) == FALSE)
		return (FALSE);
	return (cc_indent(FFRAND, n));
}

/*
 * Get the level of indentation after line lp is processed
 * Note getindent has two returns:
 * curi = value if indenting current line.
 * return value = value affecting subsequent lines.
 */
static int
getindent(const struct line *lp, int *curi)
{
	int lo, co;		/* leading space,  current offset*/
	int nicol = 0;		/* position count */
	int c = '\0';		/* current char */
	int newind = 0;		/* new index value */
	int stringp = FALSE;	/* in string? */
	int escp = FALSE;	/* Escape char? */
	int lastc = '\0';	/* Last matched string delimiter */
	int nparen = 0;		/* paren count */
	int obrace = 0;		/* open brace count */
	int cbrace = 0;		/* close brace count */
	int firstnwsp = FALSE;	/* First nonspace encountered? */
	int colonp = FALSE;	/* Did we see a colon? */
	int questionp = FALSE;	/* Did we see a question mark? */
	int slashp = FALSE;	/* Slash? */
	int astp = FALSE;	/* Asterisk? */
	int cpos = -1;		/* comment position */
	int cppp  = FALSE;	/* Preprocessor command? */

	*curi = 0;

	/* Compute leading space */
	for (lo = 0; lo < llength(lp); lo++) {
		if (!isspace(c = lgetc(lp, lo)))
			break;
		if (c == '\t')
			nicol = ntabstop(nicol, curbp->b_tabw);
		else
			nicol++;
	}

	/* If last line was blank, choose 0 */
	if (lo == llength(lp))
		nicol = 0;

	newind = 0;
	/* Compute modifiers */
	for (co = lo; co < llength(lp); co++) {
		c = lgetc(lp, co);
		/* We have a non-whitespace char */
		if (!firstnwsp && !isspace(c)) {
			if (c == '#')
				cppp = TRUE;
			firstnwsp = TRUE; 
		}
		if (c == '\\')
			escp = !escp;
		else if (stringp) {
			if (!escp && (c == '"' || c == '\'')) {
				/* unescaped string char */
				if (getmatch(c, lastc))
					stringp = FALSE;
			}
		} else if (c == '"' || c == '\'') {
			stringp = TRUE;
			lastc = c;
		} else if (c == '(') {
			nparen++;
		} else if (c == ')') {
			nparen--;
		} else if (c == '{') {
			obrace++;
			firstnwsp = FALSE;
		} else if (c == '}') {
			cbrace++;
		} else if (c == '?') {
			questionp = TRUE;
		} else if (c == ':') {
			/* ignore (foo ? bar : baz) construct */
			if (!questionp)
				colonp = TRUE;
		} else if (c == '/') {
			/* first nonwhitespace? -> indent */
			if (firstnwsp) {
				/* If previous char asterisk -> close */
				if (astp)
					cpos = -1;
				else
					slashp = TRUE;
			}
		} else if (c == '*') {
			/* If previous char slash -> open */
			if (slashp)
				cpos = co;
			else
				astp = TRUE;
		} else if (firstnwsp) {
			firstnwsp = FALSE;
		}

		/* Reset matches that apply to next character only */
		if (c != '\\')
			escp = FALSE;
		if (c != '*')
			astp = FALSE;
		if (c != '/')
			slashp = FALSE;
	}
	/*
	 * If not terminated with a semicolon, and brace or paren open.
	 * we continue
	 */
	if (colonp) {
		*curi += cc_colon_indent;
		newind -= cc_colon_indent;
	}

	*curi -= (cbrace) * cc_basic_indent;
	newind += obrace * cc_basic_indent;

	if (nparen < 0)
		newind -= cc_cont_indent;
	else if (nparen > 0)
		newind += cc_cont_indent;

	*curi += nicol;

	/* Ignore preprocessor. Otherwise, add current column */
	if (cppp) {
		newind = nicol;
		*curi = 0;
	} else {
		newind += nicol;
	}

	if (cpos != -1)
		newind = findcolpos(curbp, lp, cpos);

	return (newind);
}

/*
 * Given a delimiter and its purported mate, tell us if they
 * match.
 */
static int
getmatch(int c, int mc)
{
	int match = FALSE;

	switch (c) {
	case '"':
		match = (mc == '"');
		break;
	case '\'':
		match = (mc == '\'');
		break;
	case '(':
		match = (mc == ')');
		break;
	case '[':
		match = (mc == ']');
		break;
	case '{':
		match = (mc == '}');
		break;
	}

	return (match);
}

static int
in_whitespace(struct line *lp, int len)
{
	int lo;
	int inwhitep = FALSE;

	for (lo = 0; lo < len; lo++) {
		if (!isspace(lgetc(lp, lo)))
			break;
		if (lo == len - 1)
			inwhitep = TRUE;
	}

	return (inwhitep);
}


/* convert a line/offset pair to a column position (for indenting) */
static int
findcolpos(const struct buffer *bp, const struct line *lp, int lo)
{
	int	col, i, c;
	char tmp[5];

	/* determine column */
	col = 0;

	for (i = 0; i < lo; ++i) {
		c = lgetc(lp, i);
		if (c == '\t') {
			col = ntabstop(col, curbp->b_tabw);
		} else if (ISCTRL(c) != FALSE)
			col += 2;
		else if (isprint(c)) {
			col++;
		} else {
			col += snprintf(tmp, sizeof(tmp), "\\%o", c);
		}

	}
	return (col);
}

/*
 * Find a non-blank line, searching backwards from the supplied line pointer.
 * For C, nonblank is non-preprocessor, non C++, and accounts
 * for complete C-style comments.
 */
static struct line *
findnonblank(struct line *lp)
{
	int lo;
	int nonblankp = FALSE;
	int commentp = FALSE;
	int slashp;
	int astp;
	int c;

	while (lback(lp) != curbp->b_headp && (commentp || !nonblankp)) {
		lp = lback(lp);
		slashp = FALSE;
		astp = FALSE;

		/* Potential nonblank? */
		nonblankp = isnonblank(lp, llength(lp));

		/*
		 * Search from end, removing complete C-style
		 * comments. If one is found, ignore it and
		 * test for nonblankness from where it starts.
		 */
		for (lo = llength(lp) - 1; lo >= 0; lo--) {
			if (!isspace(c = lgetc(lp, lo))) {
				if (commentp) { /* find comment "open" */
					if (c == '*')
						astp = TRUE;
					else if (astp && c == '/') {
						commentp = FALSE;
						/* whitespace to here? */
						nonblankp = isnonblank(lp, lo);
					}
				} else { /* find comment "close" */
					if (c == '/')
						slashp = TRUE;
					else if (slashp && c == '*')
						/* found a comment */
						commentp = TRUE;
				}
			}
		}
	}

	/* Rewound to start of file? */
	if (lback(lp) == curbp->b_headp && !nonblankp)
		return (curbp->b_headp);

	return (lp);
}

/*
 * Given a line, scan forward to 'omax' and determine if we
 * are all C whitespace.
 * Note that preprocessor directives and C++-style comments
 * count as whitespace. C-style comments do not, and must
 * be handled elsewhere.
 */
static int
isnonblank(const struct line *lp, int omax)
{
	int nonblankp = FALSE;		/* Return value */
	int slashp = FALSE;		/* Encountered slash */
	int lo;				/* Loop index */
	int c;				/* char being read */

	/* Scan from front for preprocessor, C++ comments */
	for (lo = 0; lo < omax; lo++) {
		if (!isspace(c = lgetc(lp, lo))) {
			/* Possible nonblank line */
			nonblankp = TRUE;
			/* skip // and # starts */
			if (c == '#' || (slashp && c == '/')) {
				nonblankp = FALSE;
				break;
			} else if (!slashp && c == '/') {
				slashp = TRUE;
				continue;
			}
		}
		slashp = FALSE;
	}
	return (nonblankp);
}
