/*	$OpenBSD: extend.c,v 1.80 2023/04/17 10:11:30 op Exp $	*/
/* This file is in the public domain. */

/*
 *	Extended (M-x) commands, rebinding, and	startup file processing.
 */

#include <sys/types.h>
#include <regex.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chrdef.h"
#include "def.h"
#include "funmap.h"
#include "kbd.h"
#include "key.h"
#include "macro.h"

static int	 remap(KEYMAP *, int, PF, KEYMAP *);
static KEYMAP	*reallocmap(KEYMAP *);
static void	 fixmap(KEYMAP *, KEYMAP *, KEYMAP *);
static int	 dobind(KEYMAP *, const char *, int);
static char	*parsetoken(char *);
static int	 bindkey(KEYMAP **, const char *, KCHAR *, int);

/*
 * Insert a string, mainly for use from macros (created by selfinsert).
 */
int
insert(int f, int n)
{
	char	 buf[BUFSIZE], *bufp, *cp;
	int	 count, c;

	if (inmacro) {
		while (--n >= 0) {
			for (count = 0; count < maclcur->l_used; count++) {
				if ((((c = maclcur->l_text[count]) ==
				    *curbp->b_nlchr)
				    ? lnewline() : linsert(1, c)) != TRUE)
					return (FALSE);
			}
		}
		maclcur = maclcur->l_fp;
		return (TRUE);
	}
	if (n == 1)
		/* CFINS means selfinsert can tack on the end */
		thisflag |= CFINS;

	if ((bufp = eread("Insert: ", buf, sizeof(buf), EFNEW)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	while (--n >= 0) {
		cp = buf;
		while (*cp) {
			if (((*cp == *curbp->b_nlchr) ?
			    lnewline() : linsert(1, *cp))
			    != TRUE)
				return (FALSE);
			cp++;
		}
	}
	return (TRUE);
}

/*
 * Bind a key to a function.  Cases range from the trivial (replacing an
 * existing binding) to the extremely complex (creating a new prefix in a
 * map_element that already has one, so the map_element must be split,
 * but the keymap doesn't have enough room for another map_element, so
 * the keymap is reallocated).	No attempt is made to reclaim space no
 * longer used, if this is a problem flags must be added to indicate
 * malloced versus static storage in both keymaps and map_elements.
 * Structure assignments would come in real handy, but K&R based compilers
 * don't have them.  Care is taken so running out of memory will leave
 * the keymap in a usable state.
 * Parameters are:
 * curmap:  	pointer to the map being changed
 * c:		character being changed
 * funct: 	function being changed to
 * pref_map: 	if funct==NULL, map to bind to or NULL for new
 */
static int
remap(KEYMAP *curmap, int c, PF funct, KEYMAP *pref_map)
{
	int		 i, n1, n2, nold;
	KEYMAP		*mp, *newmap;
	PF		*pfp;
	struct map_element	*mep;

	if (ele >= &curmap->map_element[curmap->map_num] || c < ele->k_base) {
		if (ele > &curmap->map_element[0] && (funct != NULL ||
		    (ele - 1)->k_prefmap == NULL))
			n1 = c - (ele - 1)->k_num;
		else
			n1 = HUGE;
		if (ele < &curmap->map_element[curmap->map_num] &&
		    (funct != NULL || ele->k_prefmap == NULL))
			n2 = ele->k_base - c;
		else
			n2 = HUGE;
		if (n1 <= MAPELEDEF && n1 <= n2) {
			ele--;
			if ((pfp = calloc(c - ele->k_base + 1,
			    sizeof(PF))) == NULL)
				return (dobeep_msg("Out of memory"));

			nold = ele->k_num - ele->k_base + 1;
			for (i = 0; i < nold; i++)
				pfp[i] = ele->k_funcp[i];
			while (--n1)
				pfp[i++] = curmap->map_default;
			pfp[i] = funct;
			ele->k_num = c;
			ele->k_funcp = pfp;
		} else if (n2 <= MAPELEDEF) {
			if ((pfp = calloc(ele->k_num - c + 1,
			    sizeof(PF))) == NULL)
				return (dobeep_msg("Out of memory"));

			nold = ele->k_num - ele->k_base + 1;
			for (i = 0; i < nold; i++)
				pfp[i + n2] = ele->k_funcp[i];
			while (--n2)
				pfp[n2] = curmap->map_default;
			pfp[0] = funct;
			ele->k_base = c;
			ele->k_funcp = pfp;
		} else {
			if (curmap->map_num >= curmap->map_max) {
				if ((newmap = reallocmap(curmap)) == NULL)
					return (FALSE);
				curmap = newmap;
			}
			if ((pfp = malloc(sizeof(PF))) == NULL)
				return (dobeep_msg("Out of memory"));

			pfp[0] = funct;
			for (mep = &curmap->map_element[curmap->map_num];
			    mep > ele; mep--) {
				mep->k_base = (mep - 1)->k_base;
				mep->k_num = (mep - 1)->k_num;
				mep->k_funcp = (mep - 1)->k_funcp;
				mep->k_prefmap = (mep - 1)->k_prefmap;
			}
			ele->k_base = c;
			ele->k_num = c;
			ele->k_funcp = pfp;
			ele->k_prefmap = NULL;
			curmap->map_num++;
		}
		if (funct == NULL) {
			if (pref_map != NULL)
				ele->k_prefmap = pref_map;
			else {
				if ((mp = malloc(sizeof(KEYMAP) +
				    (MAPINIT - 1) * sizeof(struct map_element))) == NULL) {
					(void)dobeep_msg("Out of memory");
					ele->k_funcp[c - ele->k_base] =
					    curmap->map_default;
					return (FALSE);
				}
				mp->map_num = 0;
				mp->map_max = MAPINIT;
				mp->map_default = rescan;
				ele->k_prefmap = mp;
			}
		}
	} else {
		n1 = c - ele->k_base;
		if (ele->k_funcp[n1] == funct && (funct != NULL ||
		    pref_map == NULL || pref_map == ele->k_prefmap))
			/* no change */
			return (TRUE);
		if (funct != NULL || ele->k_prefmap == NULL) {
			if (ele->k_funcp[n1] == NULL)
				ele->k_prefmap = NULL;
			/* easy case */
			ele->k_funcp[n1] = funct;
			if (funct == NULL) {
				if (pref_map != NULL)
					ele->k_prefmap = pref_map;
				else {
					if ((mp = malloc(sizeof(KEYMAP) +
					    (MAPINIT - 1) *
					    sizeof(struct map_element))) == NULL) {
						(void)dobeep_msg("Out of memory");
						ele->k_funcp[c - ele->k_base] =
						    curmap->map_default;
						return (FALSE);
					}
					mp->map_num = 0;
					mp->map_max = MAPINIT;
					mp->map_default = rescan;
					ele->k_prefmap = mp;
				}
			}
		} else {
			/*
			 * This case is the splits.
			 * Determine which side of the break c goes on
			 * 0 = after break; 1 = before break
			 */
			n2 = 1;
			for (i = 0; n2 && i < n1; i++)
				n2 &= ele->k_funcp[i] != NULL;
			if (curmap->map_num >= curmap->map_max) {
				if ((newmap = reallocmap(curmap)) == NULL)
					return (FALSE);
				curmap = newmap;
			}
			if ((pfp = calloc(ele->k_num - c + !n2,
			    sizeof(PF))) == NULL)
				return (dobeep_msg("Out of memory"));

			ele->k_funcp[n1] = NULL;
			for (i = n1 + n2; i <= ele->k_num - ele->k_base; i++)
				pfp[i - n1 - n2] = ele->k_funcp[i];
			for (mep = &curmap->map_element[curmap->map_num];
			    mep > ele; mep--) {
				mep->k_base = (mep - 1)->k_base;
				mep->k_num = (mep - 1)->k_num;
				mep->k_funcp = (mep - 1)->k_funcp;
				mep->k_prefmap = (mep - 1)->k_prefmap;
			}
			ele->k_num = c - !n2;
			(ele + 1)->k_base = c + n2;
			(ele + 1)->k_funcp = pfp;
			ele += !n2;
			ele->k_prefmap = NULL;
			curmap->map_num++;
			if (pref_map == NULL) {
				if ((mp = malloc(sizeof(KEYMAP) + (MAPINIT - 1)
				    * sizeof(struct map_element))) == NULL) {
					(void)dobeep_msg("Out of memory");
					ele->k_funcp[c - ele->k_base] =
					    curmap->map_default;
					return (FALSE);
				}
				mp->map_num = 0;
				mp->map_max = MAPINIT;
				mp->map_default = rescan;
				ele->k_prefmap = mp;
			} else
				ele->k_prefmap = pref_map;
		}
	}
	return (TRUE);
}

/*
 * Reallocate a keymap. Returns NULL (without trashing the current map)
 * on failure.
 */
static KEYMAP *
reallocmap(KEYMAP *curmap)
{
	struct maps_s	*mps;
	KEYMAP	*mp;
	int	 i;

	if (curmap->map_max > SHRT_MAX - MAPGROW) {
		(void)dobeep_msg("keymap too large");
		return (NULL);
	}
	if ((mp = malloc(sizeof(KEYMAP) + (curmap->map_max + (MAPGROW - 1)) *
	    sizeof(struct map_element))) == NULL) {
		(void)dobeep_msg("Out of memory");
		return (NULL);
	}
	mp->map_num = curmap->map_num;
	mp->map_max = curmap->map_max + MAPGROW;
	mp->map_default = curmap->map_default;
	for (i = curmap->map_num; i--;) {
		mp->map_element[i].k_base = curmap->map_element[i].k_base;
		mp->map_element[i].k_num = curmap->map_element[i].k_num;
		mp->map_element[i].k_funcp = curmap->map_element[i].k_funcp;
		mp->map_element[i].k_prefmap = curmap->map_element[i].k_prefmap;
	}
	for (mps = maps; mps != NULL; mps = mps->p_next) {
		if (mps->p_map == curmap)
			mps->p_map = mp;
		else
			fixmap(curmap, mp, mps->p_map);
	}
	ele = &mp->map_element[ele - &curmap->map_element[0]];
	return (mp);
}

/*
 * Fix references to a reallocated keymap (recursive).
 */
static void
fixmap(KEYMAP *curmap, KEYMAP *mp, KEYMAP *mt)
{
	int	 i;

	for (i = mt->map_num; i--;) {
		if (mt->map_element[i].k_prefmap != NULL) {
			if (mt->map_element[i].k_prefmap == curmap)
				mt->map_element[i].k_prefmap = mp;
			else
				fixmap(curmap, mp, mt->map_element[i].k_prefmap);
		}
	}
}

/*
 * Do the input for local-set-key, global-set-key  and define-key
 * then call remap to do the work.
 */
static int
dobind(KEYMAP *curmap, const char *p, int unbind)
{
	KEYMAP	*pref_map = NULL;
	PF	 funct;
	char	 bprompt[80], *bufp, *pep;
	int	 c, s;

	if (macrodef) {
		/*
		 * Keystrokes aren't collected. Not hard, but pretty useless.
		 * Would not work for function keys in any case.
		 */
		return (dobeep_msg("Can't rebind key in macro"));
	}
	if (inmacro) {
		for (s = 0; s < maclcur->l_used - 1; s++) {
			if (doscan(curmap, c = CHARMASK(maclcur->l_text[s]), &curmap)
			    != NULL) {
				if (remap(curmap, c, NULL, NULL)
				    != TRUE)
					return (FALSE);
			}
		}
		(void)doscan(curmap, c = maclcur->l_text[s], NULL);
		maclcur = maclcur->l_fp;
	} else {
		size_t	 n;

		n = strlcpy(bprompt, p, sizeof(bprompt));
		if (n >= sizeof(bprompt))
			n = sizeof(bprompt) - 1;
		pep = bprompt + n;
		for (;;) {
			ewprintf("%s", bprompt);
			pep[-1] = ' ';
			pep = getkeyname(pep, sizeof(bprompt) -
			    (pep - bprompt), c = getkey(FALSE));
			if (doscan(curmap, c, &curmap) != NULL)
				break;
			*pep++ = '-';
			*pep = '\0';
		}
	}
	if (unbind)
		funct = rescan;
	else {
		if ((bufp = eread("%s to command: ", bprompt, sizeof(bprompt),
		    EFFUNC | EFNEW, bprompt)) == NULL)
			return (ABORT);
		else if (bufp[0] == '\0')
			return (FALSE);
		if (((funct = name_function(bprompt)) == NULL) ?
		    (pref_map = name_map(bprompt)) == NULL : funct == NULL)
			return (dobeep_msg("[No match]"));

	}
	return (remap(curmap, c, funct, pref_map));
}

/*
 * bindkey: bind key sequence to a function in the specified map.  Used by
 * excline so it can bind function keys.  To close to release to change
 * calling sequence, should just pass KEYMAP *curmap rather than
 * KEYMAP **mapp.
 */
static int
bindkey(KEYMAP **mapp, const char *fname, KCHAR *keys, int kcount)
{
	KEYMAP	*curmap = *mapp;
	KEYMAP	*pref_map = NULL;
	PF	 funct;
	int	 c;

	if (fname == NULL)
		funct = rescan;
	else if (((funct = name_function(fname)) == NULL) ?
	    (pref_map = name_map(fname)) == NULL : funct == NULL) {
		dobeep();
		ewprintf("[No match: %s]", fname);
		return (FALSE);
	}
	while (--kcount) {
		if (doscan(curmap, c = *keys++, &curmap) != NULL) {
			if (remap(curmap, c, NULL, NULL) != TRUE)
				return (FALSE);
			/*
			 * XXX - Bizzarreness. remap creates an empty KEYMAP
			 *       that the last key is supposed to point to.
			 */
			curmap = ele->k_prefmap;
		}
	}
	(void)doscan(curmap, c = *keys, NULL);
	return (remap(curmap, c, funct, pref_map));
}

/*
 * Wrapper for bindkey() that converts escapes.
 */
int
dobindkey(KEYMAP *map, const char *func, const char *str)
{
	int	 i;

	for (i = 0; *str && i < MAXKEY; i++) {
		/* XXX - convert numbers w/ strol()? */
		if (*str == '^' && *(str + 1) !=  '\0') {
			key.k_chars[i] = CCHR(toupper((unsigned char)*++str));
		} else if (*str == '\\' && *(str + 1) != '\0') {
			switch (*++str) {
			case '^':
				key.k_chars[i] = '^';
				break;
			case 't':
			case 'T':
				key.k_chars[i] = '\t';
				break;
			case 'n':
			case 'N':
				key.k_chars[i] = *curbp->b_nlchr;
				break;
			case 'r':
			case 'R':
				key.k_chars[i] = '\r';
				break;
			case 'e':
			case 'E':
				key.k_chars[i] = CCHR('[');
				break;
			case '\\':
				key.k_chars[i] = '\\';
				break;
			}
		} else
			key.k_chars[i] = *str;
		str++;
	}
	key.k_count = i;
	return (bindkey(&map, func, key.k_chars, key.k_count));
}

/*
 * This function modifies the fundamental keyboard map.
 */
int
bindtokey(int f, int n)
{
	return (dobind(fundamental_map, "Global set key: ", FALSE));
}

/*
 * This function modifies the current mode's keyboard map.
 */
int
localbind(int f, int n)
{
	return (dobind(curbp->b_modes[curbp->b_nmodes]->p_map,
	    "Local set key: ", FALSE));
}

/*
 * This function redefines a key in any keymap.
 */
int
redefine_key(int f, int n)
{
	static char	 buf[48];
	char		 tmp[32], *bufp;
	KEYMAP		*mp;

	(void)strlcpy(buf, "Define key map: ", sizeof(buf));
	if ((bufp = eread("%s", tmp, sizeof(tmp), EFNEW, buf)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	(void)strlcat(buf, tmp, sizeof(buf));
	if ((mp = name_map(tmp)) == NULL)
		return (dobeep_msgs("Unknown map", tmp));

	if (strlcat(buf, "key: ", sizeof(buf)) >= sizeof(buf))
		return (FALSE);

	return (dobind(mp, buf, FALSE));
}

int
unbindtokey(int f, int n)
{
	return (dobind(fundamental_map, "Global unset key: ", TRUE));
}

int
localunbind(int f, int n)
{
	return (dobind(curbp->b_modes[curbp->b_nmodes]->p_map,
	    "Local unset key: ", TRUE));
}

/*
 * Extended command. Call the message line routine to read in the command
 * name and apply autocompletion to it. When it comes back, look the name
 * up in the symbol table and run the command if it is found.  Print an
 * error if there is anything wrong.
 */
int
extend(int f, int n)
{
	PF	 funct;
	char	 xname[NXNAME], *bufp;

	if (!(f & FFARG))
		bufp = eread("M-x ", xname, NXNAME, EFNEW | EFFUNC);
	else
		bufp = eread("%d M-x ", xname, NXNAME, EFNEW | EFFUNC, n);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if ((funct = name_function(bufp)) != NULL) {
		if (macrodef) {
			struct line	*lp = maclcur;
			macro[macrocount - 1].m_funct = funct;
			maclcur = lp->l_bp;
			maclcur->l_fp = lp->l_fp;
			free(lp);
		}
		return ((*funct)(f, n));
	}
	return (dobeep_msg("[No match]"));
}

/*
 * Define the commands needed to do startup-file processing.
 * This code is mostly a kludge just so we can get startup-file processing.
 *
 * If you're serious about having this code, you should rewrite it.
 * To wit:
 *	It has lots of funny things in it to make the startup-file look
 *	like a GNU startup file; mostly dealing with parens and semicolons.
 *	This should all vanish.
 *
 * We define eval-expression because it's easy.	 It can make
 * *-set-key or define-key set an arbitrary key sequence, so it isn't
 * useless.
 */

/*
 * evalexpr - get one line from the user, and run it.
 * Use strlen for length of line, assume user is not typing in a '\0' in the
 * modeline. llen only used for foundparen() so old-school will be ok.
 */
int
evalexpr(int f, int n)
{
	char	 exbuf[BUFSIZE], *bufp;
	int	 llen;

	if ((bufp = eread("Eval: ", exbuf, sizeof(exbuf),
	    EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	llen = strlen(bufp);

	return (excline(exbuf, llen, 1));
}

/*
 * evalbuffer - evaluate the current buffer as line commands. Useful for
 * testing startup files.
 */
int
evalbuffer(int f, int n)
{
	struct line		*lp;
	struct buffer		*bp = curbp;
	int		 s, llen, lnum = 0;
	static char	 excbuf[BUFSIZE];

	for (lp = bfirstlp(bp); lp != bp->b_headp; lp = lforw(lp)) {
		lnum++;
		llen = llength(lp);
		if (llen >= BUFSIZE)
			return (FALSE);
		(void)strncpy(excbuf, ltext(lp), llen);

		/* make sure the line is terminated */
		excbuf[llen] = '\0';
		if ((s = excline(excbuf, llen, lnum)) != TRUE) {
			cleanup();
			return (s);
		}
	}
	cleanup();
	return (TRUE);
}

/*
 * evalfile - go get a file and evaluate it as line commands. You can
 *	go get your own startup file if need be.
 */
int
evalfile(int f, int n)
{
	FILE	*ffp = NULL;
	char	 fname[NFILEN], *bufp;
	int	 ret = 0;

	if ((bufp = eread("Load file: ", fname, NFILEN, EFNEW | EFCR)) == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		goto cleanup;
	if ((bufp = adjustname(fname, TRUE)) == NULL)
		goto cleanup;
	ret = ffropen(&ffp, bufp, NULL);
	if (ret != FIOSUC)
		goto cleanup;

	ret = load(ffp, bufp);
	(void)ffclose(ffp, NULL);
	return (ret);
cleanup:
	if (ffp) {
		if (ret == FIOGZIP)
			pclose(ffp);
		else
			fclose(ffp);
	}

	return (FALSE);
}

/*
 * load - go load the file name we got passed.
 */
int
load(FILE *ffp, const char *fname)
{
	int	 s = TRUE, line;
	int	 nbytes = 0;
	char	 excbuf[BUFSIZE];

	line = 0;
	while ((s = ffgetline(ffp, excbuf, sizeof(excbuf) - 1, &nbytes))
	    == FIOSUC) {
		line++;
		excbuf[nbytes] = '\0';
		if (excline(excbuf, nbytes, line) != TRUE) {
			s = FIOERR;
			dobeep();
			ewprintf("Error loading file %s at line %d", fname, line);
			break;
		}
	}
	excbuf[nbytes] = '\0';
	if (s != FIOEOF || (nbytes && excline(excbuf, nbytes, ++line) != TRUE))
		return (FALSE);
	return (TRUE);
}

/*
 * excline - run a line from a load file or eval-expression.
 */
int
excline(char *line, int llen, int lnum)
{
	PF	 fp;
	struct line	*lp, *np;
	int	 status, c, f, n;
	char	*funcp, *tmp;
	char	*argp = NULL;
	long	 nl;
	int	 bind;
	KEYMAP	*curmap = NULL;
#define BINDARG		0  /* this arg is key to bind (local/global set key) */
#define	BINDNO		1  /* not binding or non-quoted BINDARG */
#define BINDNEXT	2  /* next arg " (define-key) */
#define BINDDO		3  /* already found key to bind */
#define BINDEXT		1  /* space for trailing \0 */

	lp = NULL;

	if (macrodef || inmacro)
		return (dobeep_msg("Not now!"));

	f = 0;
	n = 1;
	funcp = skipwhite(line);
	if (*funcp == '\0')
		return (TRUE);	/* No error on blank lines */
	if (*funcp == '(')
		return (foundparen(funcp, llen, lnum));
	line = parsetoken(funcp);
	if (*line != '\0') {
		*line++ = '\0';
		line = skipwhite(line);
		if (ISDIGIT(*line) || *line == '-') {
			argp = line;
			line = parsetoken(line);
		}
	}
	if (argp != NULL) {
		f = FFARG;
		nl = strtol(argp, &tmp, 10);
		if (*tmp != '\0')
			return (FALSE);
		if (nl >= INT_MAX || nl <= INT_MIN)
			return (FALSE);
		n = (int)nl;
	}
	if ((fp = name_function(funcp)) == NULL)
		return (dobeep_msgs("Unknown function:", funcp));

	if (fp == bindtokey || fp == unbindtokey) {
		bind = BINDARG;
		curmap = fundamental_map;
	} else if (fp == localbind || fp == localunbind) {
		bind = BINDARG;
		curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
	} else if (fp == redefine_key)
		bind = BINDNEXT;
	else
		bind = BINDNO;
	/* Pack away all the args now... */
	if ((np = lalloc(0)) == FALSE)
		return (FALSE);
	np->l_fp = np->l_bp = maclcur = np;
	while (*line != '\0') {
		argp = skipwhite(line);
		if (*argp == '\0')
			break;
		line = parsetoken(argp);
		if (*argp != '"') {
			if (*argp == '\'')
				++argp;
			if ((lp = lalloc((int) (line - argp) + BINDEXT)) ==
			    NULL) {
				status = FALSE;
				goto cleanup;
			}
			bcopy(argp, ltext(lp), (int)(line - argp));
			/* don't count BINDEXT */
			lp->l_used--;
			if (bind == BINDARG)
				bind = BINDNO;
		} else {
			/* quoted strings are special */
			++argp;
			if (bind != BINDARG) {
				lp = lalloc((int)(line - argp) + BINDEXT);
				if (lp == NULL) {
					status = FALSE;
					goto cleanup;
				}
				lp->l_used = 0;
			} else
				key.k_count = 0;
			while (*argp != '"' && *argp != '\0') {
				if (key.k_count >= MAXKEY) {
					status = FALSE;
					goto cleanup;
				}
				if (*argp != '\\')
					c = *argp++;
				else {
					switch (*++argp) {
					case 't':
					case 'T':
						c = CCHR('I');
						break;
					case 'n':
					case 'N':
						c = CCHR('J');
						break;
					case 'r':
					case 'R':
						c = CCHR('M');
						break;
					case 'e':
					case 'E':
						c = CCHR('[');
						break;
					case '^':
						/*
						 * split into two statements
						 * due to bug in OSK cpp
						 */
						if (*++argp == '\\')
							++argp;
						c = CHARMASK(*argp);
						c = ISLOWER(c) ?
						    CCHR(TOUPPER(c)) : CCHR(c);
						break;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						c = *argp - '0';
						if (argp[1] <= '7' &&
						    argp[1] >= '0') {
							c <<= 3;
							c += *++argp - '0';
							if (argp[1] <= '7' &&
							    argp[1] >= '0') {
								c <<= 3;
								c += *++argp
								    - '0';
							}
						}
						break;
					case 'f':
					case 'F':
						c = *++argp - '0';
						if (ISDIGIT(argp[1])) {
							c *= 10;
							c += *++argp - '0';
						}
						c += KFIRST;
						break;
					default:
						c = CHARMASK(*argp);
						break;
					}
					argp++;
				}
				if (bind == BINDARG)
					key.k_chars[key.k_count++] = c;
				else
					lp->l_text[lp->l_used++] = c;
			}
			if (*line)
				line++;
		}
		switch (bind) {
		case BINDARG:
			bind = BINDDO;
			break;
		case BINDNEXT:
			lp->l_text[lp->l_used] = '\0';
			if ((curmap = name_map(lp->l_text)) == NULL) {
				(void)dobeep_msgs("No such mode:", lp->l_text);
				status = FALSE;
				free(lp);
				goto cleanup;
			}
			free(lp);
			bind = BINDARG;
			break;
		default:
			lp->l_fp = np->l_fp;
			lp->l_bp = np;
			np->l_fp = lp;
			np = lp;
		}
	}
	switch (bind) {
	default:
		(void)dobeep_msg("Bad args to set key");
		status = FALSE;
		break;
	case BINDDO:
		if (fp != unbindtokey && fp != localunbind) {
			if (!lp) {
				status = FALSE;
				goto cleanup;
			}
			lp->l_text[lp->l_used] = '\0';
			status = bindkey(&curmap, lp->l_text, key.k_chars,
			    key.k_count);
		} else
			status = bindkey(&curmap, NULL, key.k_chars,
			    key.k_count);
		break;
	case BINDNO:
		inmacro = TRUE;
		maclcur = maclcur->l_fp;
		status = (*fp)(f, n);
		inmacro = FALSE;
	}
cleanup:
	lp = maclcur->l_fp;
	while (lp != maclcur) {
		np = lp->l_fp;
		free(lp);
		lp = np;
	}
	free(lp);
	maclhead = NULL;
	macrodef = FALSE;
	return (status);
}

/*
 * a pair of utility functions for the above
 */
char *
skipwhite(char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	if ((*s == ';') || (*s == '#'))
		*s = '\0';
	return (s);
}

static char *
parsetoken(char *s)
{
	if (*s != '"') {
		while (*s && *s != ' ' && *s != '\t' && *s != ')' && *s != '(')
			s++;
		if (*s == ';')
			*s = '\0';
	} else
		do {
			/*
			 * Strings get special treatment.
			 * Beware: You can \ out the end of the string!
			 */
			if (*s == '\\')
				++s;
		} while (*++s != '"' && *s != '\0');
	return (s);
}
