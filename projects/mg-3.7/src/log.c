/*	$OpenBSD: log.c,v 1.13 2022/12/26 19:16:02 jmc Exp $	*/

/* 
 * This file is in the public domain.
 *
 * Author: Mark Lumsden <mark@showcomplex.com>
 *
 */

/*
 * Record a history of an mg session for temporal debugging.
 * Sometimes pressing a key will set the scene for a bug only visible 
 * dozens of keystrokes later. gdb has its limitations in this scenario.
 *
 * Note this file is not compiled into mg by default, you will need to
 * amend the 'Makefile' for that to happen. Because of this, the code
 * is subject to bit-rot. However, I know myself and others have 
 * written similar functionally often enough, that recording the below 
 * in a code repository could aid the development efforts of mg, even
 * if it requires a bit of effort to get working. The current code is
 * written in the spirit of debugging (quickly and perhaps not ideal,
 * but it does what is required well enough). Should debugging become
 * more formalised within mg, then I would expect that to change.
 *
 * If you open a file with long lines to run through this debugging 
 * code, you may run into problems with the 1st fprintf statement in
 * in the mglog_lines() function. mg sometimes segvs at a strlen call
 * in fprintf - possibly something to do with the format string?
 * 	"%s%p b^%p f.%p %d %d\t%c|%s\n"
 * When I get time I will look into it. But since my debugging 
 * generally revolves around a file like:
 * 
 * abc
 * def
 * ghk
 *
 * I don't experience this bug. Just note it for future investigation.
 */

#include <sys/queue.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "def.h"
#include "key.h"
#include "kbd.h"
#include "funmap.h"
#include "chrdef.h"

#include "log.h"

static char	*mglogfiles_create(FILE **, char *);
static int	 mglog_lines(PF);
static int	 mglog_undo(void);
static int	 mglog_window(void);
static int	 mglog_key(KEYMAP *map);

const char	*mglogdir;
const char	*mglogpath_lines;
const char	*mglogpath_undo;
const char	*mglogpath_window;
const char	*mglogpath_key;
const char    	*mglogpath_interpreter;
const char	*mglogpath_misc;
int		 mgloglevel;

FILE		*fd_lines;
FILE		*fd_undo;
FILE		*fd_window;
FILE		*fd_key;
FILE		*fd_interpreter;
FILE		*fd_misc;

int
mglog(PF funct, void *map)
{
	if(!mglog_lines(funct))
		ewprintf("Problem logging lines");
	if(!mglog_undo())
		ewprintf("Problem logging undo");
	if(!mglog_window())
		ewprintf("Problem logging window");
	if(!mglog_key(map))
		ewprintf("Problem logging key");

	return (TRUE);
}


static int
mglog_key(KEYMAP *map)
{
	PF		*pfp;

	if (ISWORD(*key.k_chars)) {
		fprintf(fd_key, "k_count:%d k_chars:%hd\tchr:%c\t", key.k_count,
		    *key.k_chars, CHARMASK(*key.k_chars));
	} else {
		fprintf(fd_key, "k_count:%d k_chars:%hd\t\t", key.k_count,
		    *key.k_chars);
	}
	fprintf(fd_key, "map:%p %d %d %p %hd %hd\n",
	    map,
	    map->map_num,
	    map->map_max,
	    map->map_default,
	    map->map_element->k_base,
	    map->map_element->k_num
	    );
	for (pfp = map->map_element->k_funcp; *pfp != NULL; pfp++)
		fprintf(fd_key, "%s ", function_name(*pfp));

	fprintf(fd_key, "\n\n");
	fflush(fd_key);
	return (TRUE);
}

static int
mglog_window(void)
{
	struct mgwin	*wp;
	int		 i;

	for (wp = wheadp, i = 0; wp != NULL; wp = wp->w_wndp, ++i) {
		fprintf(fd_window,
		    "%d wh%p wlst%p wbfp%p wlp%p wdtp%p wmkp%p wdto%d wmko%d" \
		    " wtpr%d wntr%d wfrm%d wrfl%c wflg%c wwrl%p wdtl%d" \
		    " wmkl%d\n",
		    i,
		    wp,
		    &wp->w_list,
		    wp->w_bufp,
		    wp->w_linep,
		    wp->w_dotp,
		    wp->w_markp,
		    wp->w_doto,
		    wp->w_marko,
		    wp->w_toprow,
		    wp->w_ntrows,
		    wp->w_frame,
		    wp->w_rflag,
		    wp->w_flag,
		    wp->w_wrapline,
		    wp->w_dotline,
		    wp->w_markline
		    );
	}
	fflush(fd_window);
	return (TRUE);
}

static int
mglog_undo(void)
{
	struct undo_rec	*rec;
	char		 buf[4096], tmp[1024];
	int      	 num;
	char		*jptr;

	jptr = "^J"; /* :) */
	/*
	 * From undo_dump()
	 */
	num = 0;
	TAILQ_FOREACH(rec, &curbp->b_undo, next) {
		num++;
		fprintf(fd_undo, "%d:\t %s at %d ", num,
		    (rec->type == DELETE) ? "DELETE":
		    (rec->type == DELREG) ? "DELREGION":
		    (rec->type == INSERT) ? "INSERT":
		    (rec->type == BOUNDARY) ? "----" :
		    (rec->type == MODIFIED) ? "MODIFIED": "UNKNOWN",
		    rec->pos
		    );
		if (rec->content) {
			(void)strlcat(buf, "\"", sizeof(buf));
			snprintf(tmp, sizeof(tmp), "%.*s",
			    *rec->content == '\n' ? 2 : rec->region.r_size,
			    *rec->content == '\n' ? jptr : rec->content);
			(void)strlcat(buf, tmp, sizeof(buf));
			(void)strlcat(buf, "\"", sizeof(buf));
		}
		snprintf(tmp, sizeof(tmp), " [%d]", rec->region.r_size);
		if (strlcat(buf, tmp, sizeof(buf)) >= sizeof(buf)) {
			dobeep();
			ewprintf("Undo record too large. Aborted.");
			return (FALSE);
		}
		fprintf(fd_undo, "%s\n", buf);
		tmp[0] = buf[0] = '\0';
	}
	fprintf(fd_undo, "\t [end-of-undo]\n\n");
	fflush(fd_undo);

	return (TRUE);
}

static int
mglog_lines(PF funct)
{
	struct line     *lp;
	char		*curline, *tmp, o;
	int		 i;

	i = 0;

	fprintf(fd_lines, "%s\n", function_name(funct));
	lp = bfirstlp(curbp);

	for(;;) {
		i++;
		curline = " ";
		o = ' ';
		if (i == curwp->w_dotline) {
			curline = ">";
			if (lp->l_used > 0 && curwp->w_doto < lp->l_used)
				o = lp->l_text[curwp->w_doto];
			else
				o = '-';
		}
		if (lp->l_size == 0)
			tmp = " ";
		else
			tmp = lp->l_text;

		/* segv on fprintf below with long lines */
		fprintf(fd_lines, "%s%p b^%p f.%p %d %d\t%c|%s\n", curline,
		    lp, lp->l_bp, lp->l_fp,
		    lp->l_size, lp->l_used, o, tmp);

		lp = lforw(lp);
		if (lp == curbp->b_headp) {
			fprintf(fd_lines, " %p b^%p f.%p [bhead]\n(EOB)\n",
			    lp, lp->l_bp, lp->l_fp);

			fprintf(fd_lines, "lines:raw:%d buf:%d wdot:%d\n\n",
			    i, curbp->b_lines, curwp->w_dotline);

			break;
		}
	}
	fflush(fd_lines);

	return (TRUE);
}

/*
 * See what the eval variable code is up to.
 */
int
mglog_isvar(
	const char* const argbuf,
	const char* const argp,
	const int 	  sizof
)
{

	fprintf(fd_interpreter, " argbuf:%s,argp:%s,sizof:%d<\n",
	    argbuf,
	    argp,
	    sizof);

	fflush(fd_interpreter);
	return (TRUE);
}

/*
 * See what the eval line code is up to.
 */
int
mglog_execbuf(
	const char* const pre,
	const char* const excbuf,
	const char* const argbuf,
    	const char* const argp,
	const int 	  last,
	const int	  inlist,
    	const char* const cmdp,
	const char* const p,
	const char* const contbuf
)
{
	fprintf(fd_interpreter, "%sexcbuf:%s,argbuf:%s,argp:%s,last:%d,inlist:%d,"\
	    "cmdp:%s,p:%s,contbuf:%s<\n",
	    pre,
	    excbuf,
	    argbuf,
	    argp,
	    last,
	    inlist,
	    cmdp,
	    p,
	    contbuf
	    );
	fflush(fd_interpreter);
	return (TRUE);
}

/*
 * Misc. logging for various subsystems
 */
int
mglog_misc(
	const char *fmt,
	...
)
{
	va_list		ap;
	int		rc;

	va_start(ap, fmt);
	rc = vfprintf(fd_misc, fmt, ap);
	va_end(ap);
	fflush(fd_misc);

	if (rc < 0)
		return (FALSE);

	return (TRUE);
}



/*
 * Make sure logging to log files can happen.
 */
int
mgloginit(void)
{
	struct stat	 sb;
	mode_t           dir_mode, f_mode, oumask;
	char		*mglogfile_lines, *mglogfile_undo, *mglogfile_window;
	char		*mglogfile_key, *mglogfile_interpreter, *mglogfile_misc;

	mglogdir = "./log/";
	mglogfile_lines = "line.log";
	mglogfile_undo = "undo.log";
	mglogfile_window = "window.log";
	mglogfile_key = "key.log";
	mglogfile_interpreter = "interpreter.log";
	mglogfile_misc = "misc.log";

	/* 
	 * Change mgloglevel for desired level of logging.
	 * log.h has relevant level info.
	 */
	mgloglevel = 1;

	oumask = umask(0);
	f_mode = 0777& ~oumask;
	dir_mode = f_mode | S_IWUSR | S_IXUSR;

	if(stat(mglogdir, &sb)) {
		if (mkdir(mglogdir, dir_mode) != 0)
			return (FALSE);
		if (chmod(mglogdir, f_mode) == -1)
			return (FALSE);
	}
	mglogpath_lines = mglogfiles_create(&fd_lines, mglogfile_lines);
	if (mglogpath_lines == NULL)
		return (FALSE);
	mglogpath_undo = mglogfiles_create(&fd_undo, mglogfile_undo);
	if (mglogpath_undo == NULL)
		return (FALSE);
	mglogpath_window = mglogfiles_create(&fd_window, mglogfile_window);
	if (mglogpath_window == NULL)
		return (FALSE);
	mglogpath_key = mglogfiles_create(&fd_key, mglogfile_key);
	if (mglogpath_key == NULL)
		return (FALSE);
	mglogpath_interpreter = mglogfiles_create(&fd_interpreter,
	    mglogfile_interpreter);
	if (mglogpath_interpreter == NULL)
		return (FALSE);
	mglogpath_misc = mglogfiles_create(&fd_misc, mglogfile_misc);
	if (mglogpath_misc == NULL)
		return (FALSE);

	return (TRUE);
}	


static char *
mglogfiles_create(FILE ** fd, char *mglogfile)
{
	char		 tmp[NFILEN], *tmp2;

	if (strlcpy(tmp, mglogdir, sizeof(tmp)) >
	    sizeof(tmp))
		return (NULL);
	if (strlcat(tmp, mglogfile, sizeof(tmp)) >
	    sizeof(tmp))
		return (NULL);
	if ((tmp2 = strndup(tmp, NFILEN)) == NULL)
		return (NULL);

	if ((*fd = fopen(tmp2, "w")) == NULL)
		return (NULL);

	return (tmp2);
}
