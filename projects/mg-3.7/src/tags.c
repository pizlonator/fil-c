/*	$OpenBSD: tags.c,v 1.27 2023/03/29 19:09:04 op Exp $	*/

/*
 * This file is in the public domain.
 *
 * Author: Sunil Nimmagadda <sunil@openbsd.org>
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"

struct ctag;

static int               addctag(char *);
static int               atbow(void);
void                     closetags(void);
static int               ctagcmp(struct ctag *, struct ctag *);
static int               loadbuffer(char *);
static int               loadtags(const char *);
static int               pushtag(char *);
static int               searchpat(char *);
static struct ctag       *searchtag(char *);
static char              *strip(char *, size_t);
static void              unloadtags(void);

#define DEFAULTFN "tags"

/* ctags(1) entries are parsed and maintained in a tree. */
struct ctag {
	RB_ENTRY(ctag) entry;
	char *tag;
	char *fname;
	char *pat;
};
RB_HEAD(tagtree, ctag) tags = RB_INITIALIZER(&tags);
RB_GENERATE(tagtree, ctag, entry, ctagcmp);

struct tagpos {
	SLIST_ENTRY(tagpos) entry;
	int    doto;
	int    dotline;
	char   *bname;
};
SLIST_HEAD(tagstack, tagpos) shead = SLIST_HEAD_INITIALIZER(shead);

int
ctagcmp(struct ctag *s, struct ctag *t)
{
	return strcmp(s->tag, t->tag);
}

/*
 * Load a tags file.  If a tags file is already loaded, ask the user to
 * retain loaded tags (i any) and unload them if the user chooses not to.
 */
int
tagsvisit(int f, int n)
{
	char fname[NFILEN], *bufp, *temp;
	
	if (getbufcwd(fname, sizeof(fname)) == FALSE)
		fname[0] = '\0';
	
	if (strlcat(fname, DEFAULTFN, sizeof(fname)) >= sizeof(fname)) {
		dobeep();
		ewprintf("Filename too long");
		return (FALSE);
	}
	
	bufp = eread("Visit tags table (default %s): ", fname,
	    NFILEN, EFFILE | EFCR | EFNEW | EFDEF, DEFAULTFN);
	if (bufp == NULL)
		return (ABORT);

	if (!RB_EMPTY(&tags)) {
		if (eyorn("Keep current list of tags table also") == FALSE) {
			ewprintf("Starting a new list of tags table");
			unloadtags();
		}
	}

	temp = bufp;
	if (temp[0] == '\0')
		temp = fname;

	return (loadtags(temp));
}

/*
 * Ask user for a tag while treating word at dot as default. Visit tags
 * file if not yet done, load tags and jump to definition of the tag.
 */
int
findtag(int f, int n)
{
	char utok[MAX_TOKEN], dtok[MAX_TOKEN];
	char *tok, *bufp;
	int  ret;

	if (curtoken(f, n, dtok) == FALSE) {
		dtok[0] = '\0';
		bufp = eread("Find tag: ", utok, MAX_TOKEN, EFNUL | EFNEW);
	} else
		bufp = eread("Find tag (default %s): ", utok, MAX_TOKEN,
		    EFNUL | EFNEW, dtok);

	if (bufp == NULL)
		return (ABORT);
	else if	(bufp[0] == '\0')
		tok = dtok;
	else
		tok = utok;
	
	if (tok[0] == '\0') {
		dobeep();
		ewprintf("There is no default tag");
		return (FALSE);
	}

	if (RB_EMPTY(&tags))
		if ((ret = tagsvisit(f, n)) != TRUE)
			return (ret);
	return pushtag(tok);
}

/*
 * Free tags tree.
 */
void
unloadtags(void)
{
	struct ctag *var, *nxt;
	
	for (var = RB_MIN(tagtree, &tags); var != NULL; var = nxt) {
		nxt = RB_NEXT(tagtree, &tags, var);
		RB_REMOVE(tagtree, &tags, var);
		/* line parsed with fparseln needs to be freed */
		free(var->tag);
		free(var);
	}
}

/*
 * Lookup tag passed in tree and if found, push current location and 
 * buffername onto stack, load the file with tag definition into a new
 * buffer and position dot at the pattern.
 */
/*ARGSUSED */
int
pushtag(char *tok)
{
	struct ctag *res;
	struct tagpos *s;
	char bname[NFILEN];
	int doto, dotline;
	
	if ((res = searchtag(tok)) == NULL)
		return (FALSE);
		
	doto = curwp->w_doto;
	dotline = curwp->w_dotline;
	/* record absolute filenames. Fixes issues when mg's cwd is not the
	 * same as buffer's directory.
	 */
	if (strlcpy(bname, curbp->b_cwd, sizeof(bname)) >= sizeof(bname)) {
		dobeep();
		ewprintf("filename too long");
		return (FALSE);
	}
	if (strlcat(bname, curbp->b_bname, sizeof(bname)) >= sizeof(bname)) {
		dobeep();
		ewprintf("filename too long");
		return (FALSE);
	}	

	if (loadbuffer(res->fname) == FALSE)
		return (FALSE);
	
	if (searchpat(res->pat) == TRUE) {
		if ((s = malloc(sizeof(struct tagpos))) == NULL) {
			dobeep();
			ewprintf("Out of memory");
			return (FALSE);
		}
		if ((s->bname = strdup(bname)) == NULL) {
			dobeep();
			ewprintf("Out of memory");
			free(s);
			return (FALSE);
		}
		s->doto = doto;
		s->dotline = dotline;
		SLIST_INSERT_HEAD(&shead, s, entry);
		return (TRUE);
	} else {
		dobeep();
		ewprintf("%s: pattern not found", res->tag);
		return (FALSE);
	}
	/* NOTREACHED */
	return (FALSE);
}

/*
 * If tag stack is not empty pop stack and jump to recorded buffer, dot.
 */
int
poptag(int f, int n)
{
	struct line *dotp;
	struct tagpos *s;
	
	if (SLIST_EMPTY(&shead)) {
		dobeep();
		ewprintf("No previous location for find-tag invocation");
		return (FALSE);
	}
	s = SLIST_FIRST(&shead);
	SLIST_REMOVE_HEAD(&shead, entry);
	if (loadbuffer(s->bname) == FALSE) {
		free(s->bname);
		free(s);
		return (FALSE);
	}
	curwp->w_dotline = s->dotline;
	curwp->w_doto = s->doto;
	
	/* storing of dotp in tagpos wouldn't work out in cases when
	 * that buffer is killed by user(dangling pointer). Explicitly
	 * traverse till dotline for correct handling. 
	 */
	dotp = curwp->w_bufp->b_headp;
	while (s->dotline--)
		dotp = dotp->l_fp;
	
	curwp->w_dotp = dotp;
	free(s->bname);
	free(s);
	return (TRUE);
}

/*
 * Parse the tags file and construct the tags tree. Remove escape 
 * characters while parsing the file.
 */
int
loadtags(const char *fn)
{
	struct stat sb;
	char *l;
	FILE *fd;
	
	if ((fd = fopen(fn, "r")) == NULL) {
		dobeep();
		ewprintf("Unable to open tags file: %s", fn);
		return (FALSE);
	}
	if (fstat(fileno(fd), &sb) == -1) {
		dobeep();
		ewprintf("fstat: %s", strerror(errno));
		fclose(fd);
		return (FALSE);
	}
	if (!S_ISREG(sb.st_mode)) {
		dobeep();
		ewprintf("Not a regular file");
		fclose(fd);
		return (FALSE);
	}
	while ((l = fparseln(fd, NULL, NULL, "\\\\\0",
	    FPARSELN_UNESCCONT | FPARSELN_UNESCREST)) != NULL) {
		if (addctag(l) == FALSE) {
			fclose(fd);
			return (FALSE);
		}
	}
	fclose(fd);
	return (TRUE);
}

/*
 * Cleanup and destroy tree and stack.
 */
void
closetags(void)
{
	struct tagpos *s;	
	
	while (!SLIST_EMPTY(&shead)) {
		s = SLIST_FIRST(&shead);
		SLIST_REMOVE_HEAD(&shead, entry);
		free(s->bname);
		free(s);
	}
	unloadtags();
}

/*
 * Strip away any special characters in pattern.
 * The pattern in ctags isn't a true regular expression. Its of the form
 * /^xxx$/ or ?^xxx$? and in some cases the "$" would be missing. Strip 
 * the leading and trailing special characters so the pattern matching
 * would be a simple string compare. Escape character is taken care by 
 * fparseln.
 */
char *
strip(char *s, size_t len)
{
	/* first strip trailing special chars */	
	s[len - 1] = '\0';
	if (s[len - 2] == '$')
		s[len - 2] = '\0';
	
	/* then strip leading special chars */
	s++;
	if (*s == '^')
		s++;
	
	return s;
}

/*
 * tags line is of the format "<tag>\t<filename>\t<pattern>". Split them
 * by replacing '\t' with '\0'. This wouldn't alter the size of malloc'ed
 * l, and can be freed during cleanup.
 */
int
addctag(char *s)
{
	struct ctag *t;
	char *l, *m;
	
	if ((t = malloc(sizeof(struct ctag))) == NULL) {
		dobeep();
		ewprintf("Out of memory");
		free(s);
		return (FALSE);
	}
	t->tag = s;
	if ((l = strchr(s, '\t')) == NULL)
		goto cleanup;
	*l++ = '\0';
	t->fname = l;
	if ((l = strchr(l, '\t')) == NULL)
		goto cleanup;
	*l++ = '\0';
	if (*l == '\0')
		goto cleanup;

	/* compat with newer exuberant ctags format */
	if ((m = strstr(l, ";\"")))
		*m = '\0';

	t->pat = strip(l, strlen(l));
	if (RB_INSERT(tagtree, &tags, t) != NULL) {
		/* ignore duplicates */
		free(t);
		free(s);
	}
	return (TRUE);
cleanup:
	free(t);
	free(s);
	return (FALSE);
}

/*
 * Search through each line of buffer for pattern.
 */
int
searchpat(char *s_pat)
{
	struct line *lp;
	int dotline;
	size_t plen;

	plen = strlen(s_pat);
	dotline = 1;
	lp = lforw(curbp->b_headp);
	while (lp != curbp->b_headp) {
		if (ltext(lp) != NULL && (int)plen <= llength(lp) &&
		    (strncmp(s_pat, ltext(lp), plen) == 0)) {
			curwp->w_doto = 0;
			curwp->w_dotp = lp;
			curwp->w_dotline = dotline;
			return (TRUE);
		} else {
			lp = lforw(lp);
			dotline++;
		}
	}
	return (FALSE);
}

/*
 * Return TRUE if dot is at beginning of a word or at beginning 
 * of line, else FALSE.
 */
int
atbow(void)
{
	if (curwp->w_doto == 0)
		return (TRUE);
	if (ISWORD(curwp->w_dotp->l_text[curwp->w_doto]) &&
	    !ISWORD(curwp->w_dotp->l_text[curwp->w_doto - 1]))
	    	return (TRUE);
	return (FALSE);
}

/*
 * Extract the word at dot without changing dot position.
 */
int
curtoken(int f, int n, char *token)
{
	struct line *odotp;
	int odoto, tdoto, odotline, size, r;
	char c;
	
	/* Underscore character is to be treated as "inword" while
	 * processing tokens unlike mg's default word traversal. Save
	 * and restore its cinfo value so that tag matching works for
	 * identifier with underscore.
	 */
	c = cinfo['_'];
	cinfo['_'] = _MG_W;
	
	odotp = curwp->w_dotp;
	odoto = curwp->w_doto;
	odotline = curwp->w_dotline;
	
	/* Move backward unless we are at the beginning of a word or at
	 * beginning of line.
	 */
	if (!atbow())
		if ((r = backword(f, n)) == FALSE)
			goto cleanup;
		
	tdoto = curwp->w_doto;

	if ((r = forwword(f, n)) == FALSE)
		goto cleanup;
	
	/* strip away leading whitespace if any like emacs. */
	while (ltext(curwp->w_dotp) && 
	    isspace(lgetc(curwp->w_dotp, tdoto)))
		tdoto++;

	size = curwp->w_doto - tdoto;
	if (size <= 0 || size >= MAX_TOKEN || 
	    ltext(curwp->w_dotp) == NULL) {
		r = FALSE;
		goto cleanup;
	}    
	strncpy(token, ltext(curwp->w_dotp) + tdoto, size);
	token[size] = '\0';
	r = TRUE;
	
cleanup:
	cinfo['_'] = c;
	curwp->w_dotp = odotp;
	curwp->w_doto = odoto;
	curwp->w_dotline = odotline;
	return (r);
}

/*
 * Search tagstree for a given token.
 */
struct ctag *
searchtag(char *tok)
{
	struct ctag t, *res;

	t.tag = tok;
	if ((res = RB_FIND(tagtree, &tags, &t)) == NULL) {
		dobeep();
		ewprintf("No tag containing %s", tok);
		return (NULL);
	}
	return res;
}

/*
 * This is equivalent to filevisit from file.c.
 * Look around to see if we can find the file in another buffer; if we
 * can't find it, create a new buffer, read in the text, and switch to
 * the new buffer. *scratch*, *grep*, *compile* needs to be handled 
 * differently from other buffers which have "filenames".
 */
int
loadbuffer(char *bname)
{
	struct buffer *bufp;
	char *adjf;

	/* check for special buffers which begin with '*' */
	if (bname[0] == '*') {
		if ((bufp = bfind(bname, FALSE)) != NULL) {
			curbp = bufp;
			return (showbuffer(bufp, curwp, WFFULL));
		} else {
			return (FALSE);
		}
	} else {	
		if ((adjf = adjustname(bname, TRUE)) == NULL)
			return (FALSE);
		if ((bufp = findbuffer(adjf)) == NULL)
			return (FALSE);
	}
	curbp = bufp;
	if (showbuffer(bufp, curwp, WFFULL) != TRUE)
		return (FALSE);
	if (bufp->b_fname[0] == '\0') {
		if (readin(adjf) != TRUE) {
			killbuffer(bufp);
			return (FALSE);
		}
	}
	return (TRUE);
}
