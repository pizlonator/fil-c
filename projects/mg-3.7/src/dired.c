/*	$OpenBSD: dired.c,v 1.101 2022/10/15 17:01:14 op Exp $	*/

/* This file is in the public domain. */

/* dired module for mg 2a
 * by Robert A. Larson
 */

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"

void		 dired_init(void);
static int	 dired(int, int);
static int	 d_otherwindow(int, int);
static int	 d_undel(int, int);
static int	 d_undelbak(int, int);
static int	 d_findfile(int, int);
static int	 d_ffotherwindow(int, int);
static int	 d_expunge(int, int);
static int	 d_copy(int, int);
static int	 d_del(int, int);
static int	 d_rename(int, int);
static int	 d_exec(int, struct buffer *, const char *, const char *, ...);
static int	 d_shell_command(int, int);
static int	 d_create_directory(int, int);
static int	 d_makename(struct line *, char *, size_t);
static int	 d_warpdot(struct line *, int *);
static int	 d_forwpage(int, int);
static int	 d_backpage(int, int);
static int	 d_forwline(int, int);
static int	 d_backline(int, int);
static int	 d_killbuffer_cmd(int, int);
static int	 d_refreshbuffer(int, int);
static int	 d_filevisitalt(int, int);
static int	 d_gotofile(int, int);
static void	 reaper(int);
static int	 gotofile(char*);
static struct buffer	*refreshbuffer(struct buffer *);
static int	 createlist(struct buffer *);
static void	 redelete(struct buffer *);
static char 	 *findfname(struct line *, char *);

extern struct keymap_s helpmap, cXmap, metamap;

const char DDELCHAR = 'D';

/*
 * Structure which holds a linked list of file names marked for
 * deletion. Used to maintain dired buffer 'state' between refreshes.
 */
struct delentry {
	SLIST_ENTRY(delentry) entry;
	char   *fn;
};
SLIST_HEAD(slisthead, delentry) delhead = SLIST_HEAD_INITIALIZER(delhead);

static PF dirednul[] = {
	setmark,		/* ^@ */
	gotobol,		/* ^A */
	backchar,		/* ^B */
	rescan,			/* ^C */
	d_del,			/* ^D */
	gotoeol,		/* ^E */
	forwchar,		/* ^F */
	ctrlg,			/* ^G */
	NULL,			/* ^H */
};

static PF diredcl[] = {
	reposition,		/* ^L */
	d_findfile,		/* ^M */
	d_forwline,		/* ^N */
	rescan,			/* ^O */
	d_backline,		/* ^P */
	rescan,			/* ^Q */
	backisearch,		/* ^R */
	forwisearch,		/* ^S */
	rescan,			/* ^T */
	universal_argument,	/* ^U */
	d_forwpage,		/* ^V */
	rescan,			/* ^W */
	NULL			/* ^X */
};

static PF diredcz[] = {
	spawncli,		/* ^Z */
	NULL,			/* esc */
	rescan,			/* ^\ */
	rescan,			/* ^] */
	rescan,			/* ^^ */
	rescan,			/* ^_ */
	d_forwline,		/* SP */
	d_shell_command,	/* ! */
	rescan,			/* " */
	rescan,			/* # */
	rescan,			/* $ */
	rescan,			/* % */
	rescan,			/* & */
	rescan,			/* ' */
	rescan,			/* ( */
	rescan,			/* ) */
	rescan,			/* * */
	d_create_directory	/* + */
};

static PF direda[] = {
	d_filevisitalt,		/* a */
	rescan,			/* b */
	d_copy,			/* c */
	d_del,			/* d */
	d_findfile,		/* e */
	d_findfile,		/* f */
	d_refreshbuffer,	/* g */
	rescan,			/* h */
	rescan,			/* i */
	d_gotofile		/* j */
};

static PF diredn[] = {
	d_forwline,		/* n */
	d_ffotherwindow,	/* o */
	d_backline,		/* p */
	d_killbuffer_cmd,	/* q */
	d_rename,		/* r */
	rescan,			/* s */
	rescan,			/* t */
	d_undel,		/* u */
	rescan,			/* v */
	rescan,			/* w */
	d_expunge		/* x */
};

static PF direddl[] = {
	d_undelbak		/* del */
};

static PF diredbp[] = {
	d_backpage		/* v */
};

static PF dirednull[] = {
	NULL
};

static struct KEYMAPE (1) d_backpagemap = {
	1,
	1,
	rescan,
	{
		{
		'v', 'v', diredbp, NULL
		}
	}
};

static struct KEYMAPE (7) diredmap = {
	7,
	7,
	rescan,
	{
		{
			CCHR('@'), CCHR('H'), dirednul, (KEYMAP *) & helpmap
		},
		{
			CCHR('L'), CCHR('X'), diredcl, (KEYMAP *) & cXmap
		},
		{
			CCHR('['), CCHR('['), dirednull, (KEYMAP *) &
			d_backpagemap
		},
		{
			CCHR('Z'), '+', diredcz, (KEYMAP *) & metamap
		},
		{
			'a', 'j', direda, NULL
		},
		{
			'n', 'x', diredn, NULL
		},
		{
			CCHR('?'), CCHR('?'), direddl, NULL
		},
	}
};

void
dired_init(void)
{
	funmap_add(dired, "dired", 1);
	funmap_add(d_create_directory, "dired-create-directory", 1);
	funmap_add(d_copy, "dired-do-copy", 1);
	funmap_add(d_expunge, "dired-do-flagged-delete", 0);
	funmap_add(d_rename, "dired-do-rename", 1);
	funmap_add(d_findfile, "dired-find-file", 1);
	funmap_add(d_ffotherwindow, "dired-find-file-other-window", 1);
	funmap_add(d_del, "dired-flag-file-deletion", 0);
	funmap_add(d_gotofile, "dired-goto-file", 1);
	funmap_add(d_forwline, "dired-next-line", 0);
	funmap_add(d_otherwindow, "dired-other-window", 0);
	funmap_add(d_backline, "dired-previous-line", 0);
	funmap_add(d_refreshbuffer, "dired-revert", 0);
	funmap_add(d_backpage, "dired-scroll-down", 0);
	funmap_add(d_forwpage, "dired-scroll-up", 0);
	funmap_add(d_shell_command, "dired-shell-command", 1);
	funmap_add(d_undel, "dired-unmark", 0);
	funmap_add(d_undelbak, "dired-unmark-backward", 0);
	funmap_add(d_killbuffer_cmd, "quit-window", 0);
	maps_add((KEYMAP *)&diredmap, "dired");
	dobindkey(fundamental_map, "dired", "^Xd");
}

int
dired(int f, int n)
{
	char		 dname[NFILEN], *bufp, *slash;
	struct buffer	*bp;

	if (curbp->b_fname[0] != '\0') {
		(void)strlcpy(dname, curbp->b_fname, sizeof(dname));
		if ((slash = strrchr(dname, '/')) != NULL) {
			*(slash + 1) = '\0';
		}
	} else {
		if (getcwd(dname, sizeof(dname)) == NULL)
			dname[0] = '\0';
	}

	if ((bufp = eread("Dired (directory): ", dname, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		return (FALSE);
	if ((bp = dired_(bufp)) == NULL)
		return (FALSE);

	curbp = bp;
	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

int
d_otherwindow(int f, int n)
{
	char		 dname[NFILEN], *bufp, *slash;
	struct buffer	*bp;
	struct mgwin	*wp;

	if (curbp->b_fname[0] != '\0') {
		(void)strlcpy(dname, curbp->b_fname, sizeof(dname));
		if ((slash = strrchr(dname, '/')) != NULL) {
			*(slash + 1) = '\0';
		}
	} else {
		if (getcwd(dname, sizeof(dname)) == NULL)
			dname[0] = '\0';
	}

	if ((bufp = eread("Dired other window: ", dname, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if ((bp = dired_(bufp)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	return (TRUE);
}

int
d_del(int f, int n)
{
	if (n < 0)
		return (FALSE);
	while (n--) {
		if (d_warpdot(curwp->w_dotp, &curwp->w_doto) == TRUE) {
			lputc(curwp->w_dotp, 0, DDELCHAR);
			curbp->b_flag |= BFDIREDDEL;
		}
		if (lforw(curwp->w_dotp) != curbp->b_headp) {
			curwp->w_dotp = lforw(curwp->w_dotp);
			curwp->w_dotline++;
		}
	}
	curwp->w_rflag |= WFEDIT | WFMOVE;
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

int
d_undel(int f, int n)
{
	if (n < 0)
		return (d_undelbak(f, -n));
	while (n--) {
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, ' ');
		if (lforw(curwp->w_dotp) != curbp->b_headp) {
			curwp->w_dotp = lforw(curwp->w_dotp);
			curwp->w_dotline++;
		}
	}
	curwp->w_rflag |= WFEDIT | WFMOVE;
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

int
d_undelbak(int f, int n)
{
	if (n < 0)
		return (d_undel(f, -n));
	while (n--) {
		if (lback(curwp->w_dotp) != curbp->b_headp) {
			curwp->w_dotp = lback(curwp->w_dotp);
			curwp->w_dotline--;
		}
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, ' ');
	}
	curwp->w_rflag |= WFEDIT | WFMOVE;
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

int
d_findfile(int f, int n)
{
	struct buffer	*bp;
	int		 s;
	char		 fname[NFILEN];

	if ((s = d_makename(curwp->w_dotp, fname, sizeof(fname))) == ABORT)
		return (FALSE);
	if (s == TRUE)
		bp = dired_(fname);
	else
		bp = findbuffer(fname);
	if (bp == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFFULL) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] != 0)
		return (TRUE);
	return (readin(fname));
}

int
d_ffotherwindow(int f, int n)
{
	char		 fname[NFILEN];
	int		 s;
	struct buffer	*bp;
	struct mgwin	*wp;

	if ((s = d_makename(curwp->w_dotp, fname, sizeof(fname))) == ABORT)
		return (FALSE);
	if ((bp = (s ? dired_(fname) : findbuffer(fname))) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] != 0)
		return (TRUE);	/* never true for dired buffers */
	return (readin(fname));
}

int
d_expunge(int f, int n)
{
	struct line	*lp, *nlp;
	char		 fname[NFILEN], sname[NFILEN];
	int		 tmp;

	tmp = curwp->w_dotline;
	curwp->w_dotline = 0;

	for (lp = bfirstlp(curbp); lp != curbp->b_headp; lp = nlp) {
		curwp->w_dotline++;
		nlp = lforw(lp);
		if (llength(lp) && lgetc(lp, 0) == 'D') {
			switch (d_makename(lp, fname, sizeof(fname))) {
			case ABORT:
				dobeep();
				ewprintf("Bad line in dired buffer");
				curwp->w_dotline = tmp;
				return (FALSE);
			case FALSE:
				if (unlink(fname) == -1) {
					(void)xbasename(sname, fname, NFILEN);
					dobeep();
					ewprintf("Could not delete '%s'", sname);
					curwp->w_dotline = tmp;
					return (FALSE);
				}
				break;
			case TRUE:
				if (rmdir(fname) == -1) {
					(void)xbasename(sname, fname, NFILEN);
					dobeep();
					ewprintf("Could not delete directory "
					    "'%s'", sname);
					curwp->w_dotline = tmp;
					return (FALSE);
				}
				break;
			}
			lfree(lp);
			curwp->w_bufp->b_lines--;
			if (tmp > curwp->w_dotline)
				tmp--;
			curwp->w_rflag |= WFFULL;
		}
	}
	curwp->w_dotline = tmp;
	d_warpdot(curwp->w_dotp, &curwp->w_doto);

	/* we have deleted all items successfully, remove del flag */
	curbp->b_flag &= ~BFDIREDDEL;

	return (TRUE);
}

int
d_copy(int f, int n)
{
	struct stat      statbuf;
	char		 frname[NFILEN], toname[NFILEN], sname[NFILEN];
	char		*topath, *bufp;
	int		 ret;
	size_t		 off;
	struct buffer	*bp;

	if (d_makename(curwp->w_dotp, frname, sizeof(frname)) != FALSE) {
		dobeep();
		ewprintf("Not a file");
		return (FALSE);
	}
	off = strlcpy(toname, curbp->b_fname, sizeof(toname));
	if (off >= sizeof(toname) - 1) {	/* can't happen, really */
		dobeep();
		ewprintf("Directory name too long");
		return (FALSE);
	}
	(void)xbasename(sname, frname, NFILEN);
	bufp = eread("Copy %s to: ", toname, sizeof(toname),
	    EFDEF | EFNEW | EFCR, sname);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	topath = adjustname(toname, TRUE);
	if (topath && stat(topath, &statbuf) == 0) {
		if (S_ISDIR(statbuf.st_mode)) {
			ret = snprintf(toname, sizeof(toname), "%s/%s",
			    topath, sname);
			if (ret < 0 || ret >= (int)sizeof(toname) - 1) {
				dobeep();
				ewprintf("Directory name too long");
				return (FALSE);
			}
			topath = adjustname(toname, TRUE);
		}
	}

	if (topath == NULL)
		return (FALSE);

	if (strcmp(frname, topath) == 0) {
		ewprintf("Cannot copy to same file: %s", frname);
		return (TRUE);
	}
	ret = (copy(frname, topath) >= 0) ? TRUE : FALSE;
	if (ret != TRUE)
		return (ret);
	if ((bp = refreshbuffer(curbp)) == NULL)
		return (FALSE);

	ewprintf("Copy: 1 file");
	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

int
d_rename(int f, int n)
{
	struct stat      statbuf;
	char		 frname[NFILEN], toname[NFILEN];
	char		*topath, *bufp;
	int		 ret;
	size_t		 off;
	struct buffer	*bp;
	char		 sname[NFILEN];

	if (d_makename(curwp->w_dotp, frname, sizeof(frname)) != FALSE) {
		dobeep();
		ewprintf("Not a file");
		return (FALSE);
	}
	off = strlcpy(toname, curbp->b_fname, sizeof(toname));
	if (off >= sizeof(toname) - 1) {	/* can't happen, really */
		dobeep();
		ewprintf("Directory name too long");
		return (FALSE);
	}
	(void)xbasename(sname, frname, NFILEN);
	bufp = eread("Rename %s to: ", toname,
	    sizeof(toname), EFDEF | EFNEW | EFCR, sname);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	topath = adjustname(toname, TRUE);
	if (topath && stat(topath, &statbuf) == 0) {
		if (S_ISDIR(statbuf.st_mode)) {
			ret = snprintf(toname, sizeof(toname), "%s/%s",
			    topath, sname);
			if (ret < 0 || ret >= (int)sizeof(toname) - 1) {
				dobeep();
				ewprintf("Directory name too long");
				return (FALSE);
			}
			topath = adjustname(toname, TRUE);
		}
	}
	if (topath == NULL)
		return (FALSE);

	if (strcmp(frname, topath) == 0) {
		ewprintf("Cannot move to same file: %s", frname);
		return (TRUE);
	}
	ret = (rename(frname, topath) >= 0) ? TRUE : FALSE;
	if (ret != TRUE)
		return (ret);
	if ((bp = refreshbuffer(curbp)) == NULL)
		return (FALSE);

	ewprintf("Move: 1 file");
	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

void
reaper(int signo __attribute__((unused)))
{
	int	save_errno = errno, status;

	while (waitpid(-1, &status, WNOHANG) >= 0)
		;
	errno = save_errno;
}

/*
 * Pipe the currently selected file through a shell command.
 */
int
d_shell_command(int f, int n)
{
	char		 command[512], fname[PATH_MAX], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;
	char		 sname[NFILEN];

	bp = bfind("*Shell Command Output*", TRUE);
	if (bclear(bp) != TRUE)
		return (ABORT);

	if (d_makename(curwp->w_dotp, fname, sizeof(fname)) != FALSE) {
		dobeep();
		ewprintf("bad line");
		return (ABORT);
	}

	command[0] = '\0';
	(void)xbasename(sname, fname, NFILEN);
	bufp = eread("! on %s: ", command, sizeof(command), EFNEW, sname);
	if (bufp == NULL)
		return (ABORT);

	if (d_exec(0, bp, fname, "sh", "-c", command, NULL) != TRUE)
		return (ABORT);

	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (ABORT);	/* XXX - free the buffer?? */
	curwp = wp;
	curbp = wp->w_bufp;
	return (TRUE);
}

/*
 * Pipe input file to cmd and insert the command's output in the
 * given buffer.  Each line will be prefixed with the given
 * number of spaces.
 */
static int
d_exec(int space, struct buffer *bp, const char *input, const char *cmd, ...)
{
	char	 buf[BUFSIZ];
	va_list	 ap;
	struct	 sigaction olda, newa;
	char	**argv = NULL, *cp;
	FILE	*fin;
	int	 fds[2] = { -1, -1 };
	int	 infd = -1;
	int	 ret = (ABORT), n;
	pid_t	 pid;

	if (sigaction(SIGCHLD, NULL, &olda) == -1)
		return (ABORT);

	/* Find the number of arguments. */
	va_start(ap, cmd);
	for (n = 2; va_arg(ap, char *) != NULL; n++)
		;
	va_end(ap);

	/* Allocate and build the argv. */
	if ((argv = calloc(n, sizeof(*argv))) == NULL) {
		dobeep();
		ewprintf("Can't allocate argv : %s", strerror(errno));
		goto out;
	}

	n = 1;
	argv[0] = (char *)cmd;
	va_start(ap, cmd);
	while ((argv[n] = va_arg(ap, char *)) != NULL)
		n++;
	va_end(ap);

	if (input == NULL)
		input = "/dev/null";

	if ((infd = open(input, O_RDONLY)) == -1) {
		dobeep();
		ewprintf("Can't open input file : %s", strerror(errno));
		goto out;
	}

	if (pipe(fds) == -1) {
		dobeep();
		ewprintf("Can't create pipe : %s", strerror(errno));
		goto out;
	}

	newa.sa_handler = reaper;
	newa.sa_flags = 0;
	if (sigaction(SIGCHLD, &newa, NULL) == -1)
		goto out;

	if ((pid = fork()) == -1) {
		dobeep();
		ewprintf("Can't fork");
		goto out;
	}

	switch (pid) {
	case 0: /* Child */
		close(fds[0]);
		dup2(infd, STDIN_FILENO);
		dup2(fds[1], STDOUT_FILENO);
		dup2(fds[1], STDERR_FILENO);
		if (execvp(argv[0], argv) == -1)
			ewprintf("Can't exec %s: %s", argv[0], strerror(errno));
		exit(1);
		break;
	default: /* Parent */
		close(infd);
		close(fds[1]);
		infd = fds[1] = -1;
		if ((fin = fdopen(fds[0], "r")) == NULL)
			goto out;
		while (fgets(buf, sizeof(buf), fin) != NULL) {
			cp = strrchr(buf, *bp->b_nlchr);
			if (cp == NULL && !feof(fin)) {	/* too long a line */
				int c;
				addlinef(bp, "%*s%s...", space, "", buf);
				while ((c = getc(fin)) != EOF &&
				    c != *bp->b_nlchr)
					;
				continue;
			} else if (cp)
				*cp = '\0';
			addlinef(bp, "%*s%s", space, "", buf);
		}
		fclose(fin);
		break;
	}
	ret = (TRUE);

out:
	if (sigaction(SIGCHLD, &olda, NULL) == -1)
		ewprintf("Warning, couldn't reset previous signal handler");
	if (fds[0] != -1)
		close(fds[0]);
	if (fds[1] != -1)
		close(fds[1]);
	if (infd != -1)
		close(infd);
	free(argv);
	return ret;
}

int
d_create_directory(int f, int n)
{
	int ret;
	struct buffer	*bp;

	ret = ask_makedir();
	if (ret != TRUE)
		return(ret);

	if ((bp = refreshbuffer(curbp)) == NULL)
		return (FALSE);

	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

int
d_killbuffer_cmd(int f, int n)
{
	return(killbuffer_cmd(FFRAND, 0));
}

int
d_refreshbuffer(int f, int n)
{
	struct buffer *bp;

	if ((bp = refreshbuffer(curbp)) == NULL)
		return (FALSE);

	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

/*
 * Kill then re-open the requested dired buffer.
 * If required, take a note of any files marked for deletion. Then once
 * the buffer has been re-opened, remark the same files as deleted.
 */
struct buffer *
refreshbuffer(struct buffer *bp)
{
	char		*tmp_b_fname;
	int	 	 i, tmp_w_dotline, ddel = 0;

	/* remember directory path to open later */
	tmp_b_fname = strdup(bp->b_fname);
	if (tmp_b_fname == NULL) {
		dobeep();
		ewprintf("Out of memory");
		return (NULL);
	}
	tmp_w_dotline = curwp->w_dotline;

	/* create a list of files for deletion */
	if (bp->b_flag & BFDIREDDEL)
		ddel = createlist(bp);

	killbuffer(bp);

	/* dired_() uses findbuffer() to create new buffer */
	if ((bp = dired_(tmp_b_fname)) == NULL) {
		free(tmp_b_fname);
		return (NULL);
	}
	free(tmp_b_fname);

	/* remark any previously deleted files with a 'D' */
	if (ddel)
		redelete(bp);		

	/* find dot line */
	bp->b_dotp = bfirstlp(bp);
	if (tmp_w_dotline > bp->b_lines)
		tmp_w_dotline = bp->b_lines - 1;
	for (i = 1; i < tmp_w_dotline; i++)
		bp->b_dotp = lforw(bp->b_dotp);

	bp->b_dotline = i;
	bp->b_doto = 0;
	d_warpdot(bp->b_dotp, &bp->b_doto);

	curbp = bp;

	return (bp);
}

static int
d_makename(struct line *lp, char *fn, size_t len)
{
	int	 start, nlen, ret;
	char	*namep;

	if (d_warpdot(lp, &start) == FALSE)
		return (ABORT);
	namep = &lp->l_text[start];
	nlen = llength(lp) - start;

	ret = snprintf(fn, len, "%s%.*s", curbp->b_fname, nlen, namep);
	if (ret < 0 || ret >= (int)len)
		return (ABORT); /* Name is too long. */

	/* Return TRUE if the entry is a directory. */
	return ((lgetc(lp, 2) == 'd') ? TRUE : FALSE);
}

#define NAME_FIELD	9

static int
d_warpdot(struct line *dotp, int *doto)
{
	char *tp = dotp->l_text;
	int off = 0, field = 0, len;

	/*
	 * Find the byte offset to the (space-delimited) filename
	 * field in formatted ls output.
	 */
	len = llength(dotp);
	while (off < len) {
		if (tp[off++] == ' ') {
			if (++field == NAME_FIELD) {
				*doto = off;
				return (TRUE);
			}
			/* Skip the space. */
			while (off < len && tp[off] == ' ')
				off++;
		}
	}
	/* We didn't find the field. */
	*doto = 0;
	return (FALSE);
}

static int
d_forwpage(int f, int n)
{
	forwpage(f | FFRAND, n);
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

static int
d_backpage (int f, int n)
{
	backpage(f | FFRAND, n);
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

static int
d_forwline (int f, int n)
{
	forwline(f | FFRAND, n);
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

static int
d_backline (int f, int n)
{
	backline(f | FFRAND, n);
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

int
d_filevisitalt (int f, int n)
{
	char	 fname[NFILEN];

	if (d_makename(curwp->w_dotp, fname, sizeof(fname)) == ABORT)
		return (FALSE);

	return(do_filevisitalt(fname));
}

/*
 * XXX dname needs to have enough place to store an additional '/'.
 */
struct buffer *
dired_(char *dname)
{
	struct buffer	*bp;
	int		 i;
	size_t		 len;

	if ((dname = adjustname(dname, TRUE)) == NULL) {
		dobeep();
		ewprintf("Bad directory name");
		return (NULL);
	}
	/* this should not be done, instead adjustname() should get a flag */
	len = strlen(dname);
	if (dname[len - 1] != '/') {
		dname[len++] = '/';
		dname[len] = '\0';
	}
	if ((access(dname, R_OK | X_OK)) == -1) {
		if (errno == EACCES) {
			dobeep();
			ewprintf("Permission denied: %s", dname);
		} else {
			dobeep();
			ewprintf("Error opening: %s", dname);
		}
		return (NULL);
	}
	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (strcmp(bp->b_fname, dname) == 0) {
			if (fchecktime(bp) != TRUE)
				ewprintf("Directory has changed on disk;"
				    " type g to update Dired");
			return (bp);
		}

	}
	bp = bfind(dname, TRUE);
	bp->b_flag |= BFREADONLY | BFIGNDIRTY;

	if ((d_exec(2, bp, NULL, "ls", "-al", dname, NULL)) != TRUE)
		return (NULL);

	/* Find the line with ".." on it. */
	bp->b_dotp = bfirstlp(bp);
	bp->b_dotline = 1;
	for (i = 0; i < bp->b_lines; i++) {
		bp->b_dotp = lforw(bp->b_dotp);
		bp->b_dotline++;
		if (d_warpdot(bp->b_dotp, &bp->b_doto) == FALSE)
			continue;
		if (strcmp(ltext(bp->b_dotp) + bp->b_doto, "..") == 0)
			break;
	}

	/* We want dot on the entry right after "..", if possible. */
	if (++i < bp->b_lines - 2) {
		bp->b_dotp = lforw(bp->b_dotp);
		bp->b_dotline++;
	}
	d_warpdot(bp->b_dotp, &bp->b_doto);

	(void)strlcpy(bp->b_fname, dname, sizeof(bp->b_fname));
	(void)strlcpy(bp->b_cwd, dname, sizeof(bp->b_cwd));
	if ((bp->b_modes[1] = name_mode("dired")) == NULL) {
		bp->b_modes[0] = name_mode("fundamental");
		dobeep();
		ewprintf("Could not find mode dired");
		return (NULL);
	}
	(void)fupdstat(bp);
	bp->b_nmodes = 1;
	return (bp);
}

/*
 * Iterate through the lines of the dired buffer looking for files
 * collected in the linked list made in createlist(). If a line is found
 * replace 'D' as first char in a line. As lines are found, remove the
 * corresponding item from the linked list. Iterate for as long as there
 * are items in the linked list or until end of buffer is found.
 */
void
redelete(struct buffer *bp)
{
	struct delentry	*dt, *d1 = NULL;
	struct line	*lp, *nlp;
	char		 fname[NFILEN];
	char		*p = fname;
	size_t		 plen, fnlen;
	int		 finished = 0;

	/* reset the deleted file buffer flag until a deleted file is found */
	bp->b_flag &= ~BFDIREDDEL;

	for (lp = bfirstlp(bp); lp != bp->b_headp; lp = nlp) {	
		bp->b_dotp = lp;
		if ((p = findfname(lp, p)) == NULL) {
			nlp = lforw(lp);
			continue;
		}
		plen = strlen(p);
		SLIST_FOREACH_SAFE(d1, &delhead, entry, dt) {
			fnlen = strlen(d1->fn);
			if ((plen == fnlen) && 
			    (strncmp(p, d1->fn, plen) == 0)) {
				lputc(bp->b_dotp, 0, DDELCHAR);
				bp->b_flag |= BFDIREDDEL;
				SLIST_REMOVE(&delhead, d1, delentry, entry);
				if (SLIST_EMPTY(&delhead)) {
					finished = 1;
					break;
				}	
			}
		}
		if (finished)
			break;
		nlp = lforw(lp);
	}
	while (!SLIST_EMPTY(&delhead)) {
		d1 = SLIST_FIRST(&delhead);
		SLIST_REMOVE_HEAD(&delhead, entry);
		free(d1->fn);
		free(d1);
	}
	return;
}

/*
 * Create a list of files marked for deletion.
 */
int
createlist(struct buffer *bp)
{
	struct delentry	*d1 = NULL, *d2;
	struct line	*lp, *nlp;
	char		 fname[NFILEN];
	char		*p = fname;
	int		 ret = FALSE;

	for (lp = bfirstlp(bp); lp != bp->b_headp; lp = nlp) {
		/* 
		 * Check if the line has 'D' on the first char and if a valid
		 * filename can be extracted from it.
		 */
		if (((lp->l_text[0] != DDELCHAR)) ||
		    ((p = findfname(lp, p)) == NULL)) {
			nlp = lforw(lp);
			continue;
		}
		if (SLIST_EMPTY(&delhead)) {
			if ((d1 = malloc(sizeof(struct delentry)))
			     == NULL)
				return (ABORT);
			if ((d1->fn = strdup(p)) == NULL) {
				free(d1);
				return (ABORT);
			}
			SLIST_INSERT_HEAD(&delhead, d1, entry);
		} else {
			if ((d2 = malloc(sizeof(struct delentry)))
			     == NULL) {
				if (d1) {
					free(d1->fn);
					free(d1);
				}
				return (ABORT);
			}
			if ((d2->fn = strdup(p)) == NULL) {
				if (d1) {
					free(d1->fn);
					free(d1);
				}
				free(d2);
				return (ABORT);
			}
			if (!d1)
				SLIST_INSERT_HEAD(&delhead, d2, entry);
			else
				SLIST_INSERT_AFTER(d1, d2, entry);
			d1 = d2;				
		}
		ret = TRUE;
		nlp = lforw(lp);
	}
	return (ret);
}

int
dired_jump(int f, int n)
{
	struct buffer   *bp;
	const char	*modename;
	char             dname[NFILEN], *fname;
	int              ret, i;

	/*
	 * We use fundamental mode in dired, so just check we aren't in
	 * dired mode for this specific function. Seems like a corner
	 * case at the moment.
	 */
	for (i = 0; i <= curbp->b_nmodes; i++) {
		modename = curbp->b_modes[i]->p_name;
		if (strncmp(modename, "dired", 5) == 0)
			return (dobeep_msg("In dired mode already"));
	}

	if (getbufcwd(dname, sizeof(dname)) != TRUE)
		return (FALSE);

	fname = curbp->b_fname;

	if ((bp = dired_(dname)) == NULL)
		return (FALSE);
	curbp = bp;

	ret = showbuffer(bp, curwp, WFFULL | WFMODE);
	if (ret != TRUE)
		return ret;

	fname = adjustname(fname, TRUE);
	if (fname != NULL)
		gotofile(fname);

	return (TRUE);
}

int
d_gotofile(int f, int n)
{
	size_t		 lenfpath;
	char		 fpath[NFILEN];
	char		*fpth, *fnp = NULL;

	if (getbufcwd(fpath, sizeof(fpath)) != TRUE)
		fpath[0] = '\0';
	lenfpath = strlen(fpath);
	fnp = eread("Goto file: ", fpath, NFILEN,
	    EFNEW | EFCR | EFFILE | EFDEF);
	if (fnp == NULL)
		return (ABORT);
	else if (fnp[0] == '\0')
		return (FALSE);

	fpth = adjustname(fpath, TRUE);		/* Removes last '/' if	*/
	if (fpth == NULL || strlen(fpth) == lenfpath - 1) {	/* directory, hence -1.	*/
		ewprintf("No file to find");	/* Current directory given so  */
		return (TRUE);			/* return at present location. */
	}
	return gotofile(fpth);
}

int
gotofile(char *fpth)
{
	struct line	*lp, *nlp;
	char		 fname[NFILEN];
	char		*p = NULL;
	int		 tmp;

	(void)xbasename(fname, fpth, NFILEN);
	tmp = 0;
	for (lp = bfirstlp(curbp); lp != curbp->b_headp; lp = nlp) {
		tmp++;
		if ((p = findfname(lp, p)) == NULL) {
			nlp = lforw(lp);
			continue;
		}
		if (strcmp(fname, p) == 0) {
			curwp->w_dotp = lp;
			curwp->w_dotline = tmp;
			(void)d_warpdot(curwp->w_dotp, &curwp->w_doto);
			tmp--;
			break;
		}
		nlp = lforw(lp);
	}
	if (tmp == curbp->b_lines - 1) {
		ewprintf("File not found %s", fname);
		return (FALSE);
	} else {
		eerase();
		return (TRUE);
	}
}

/*
 * Look for and extract a file name on a dired buffer line.
 */
char *
findfname(struct line *lp, char *fn)
{
	int start;

	(void)d_warpdot(lp, &start);
	if (start < 1)
		return NULL;
	fn = &lp->l_text[start];
	return fn;
}
