/*      $OpenBSD: interpreter.c,v 1.35 2023/04/17 10:11:30 op Exp $	*/
/*
 * This file is in the public domain.
 *
 * Author: Mark Lumsden <mark@showcomplex.com>
 */

/*
 * This file attempts to add some 'scripting' functionality into mg.
 *
 * The initial goal is to give mg the ability to use its existing functions
 * and structures in a linked-up way. Hopefully resulting in user definable
 * functions. The syntax is 'scheme' like but currently it is not a scheme
 * interpreter.
 *
 * At the moment there is no manual page reference to this file. The code below
 * is liable to change, so use at your own risk!
 *
 * If you do want to do some testing, you can add some lines to your .mg file
 * like:
 * 
 * 1. Give multiple arguments to a function that usually would accept only one:
 * (find-file "a.txt" "b.txt" "c.txt")
 *
 * 2. Define a single value variable:
 * (define myfile "d.txt")
 *
 * 3. Define a list:
 * (define myfiles(list "e.txt" "f.txt"))
 *
 * 4. Use the previously defined variable or list:
 * (find-file myfiles)
 *
 * To do:
 * 1. multiline parsing - currently only single lines supported.
 * 2. parsing for '(' and ')' throughout whole string and evaluate correctly.
 * 3. conditional execution.
 * 4. have memory allocated dynamically for variable values.
 * 5. do symbol names need more complex regex patterns? [A-Za-z][.0-9_A-Z+a-z-]
 *    at the moment.
 * 6. display line numbers with parsing errors.
 * 7. oh so many things....
 * [...]
 * n. implement user definable functions.
 * 
 * Notes:
 * - Currently calls to excline() from this file have the line length and
 *   line number set to zero.
 *   That's because excline() uses '\0' as the end of line indicator
 *   and only the call to foundparen() within excline() uses excline's 2nd
 *   and 3rd arguments.
 *   Importantly, any lines sent to there from here will not be
 *   coming back here.
 */

#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "funmap.h"

#ifdef  MGLOG
#include "kbd.h"
#include "log.h"
#endif

static int	 multiarg(char *, char *, int);
static int	 isvar(char **, char **, int);
/*static int	 dofunc(char **, char **, int);*/
static int	 founddef(char *, int, int, int, int);
static int	 foundlst(char *, int, int, int);
static int	 expandvals(char *, char *, char *);
static int	 foundfun(char *, int);
static int	 doregex(char *, char *);
static void	 clearexp(void);
static int	 parse(char *, const char *, const char *, int, int, int, int);
static int	 parsdef(char *, const char *, const char *, int, int, int);
static int	 parsval(char *, const char *, const char *, int, int, int);
static int	 parsexp(char *, const char *, const char *, int, int, int);

static int	 exitinterpreter(char *, char *, int);

TAILQ_HEAD(exphead, expentry) ehead;
struct expentry {
	TAILQ_ENTRY(expentry) eentry;
	char	*fun;		/* The 1st string found between parens.   */
	char	 funbuf[BUFSIZE];
	const char	*par1;	/* Parenthesis at start of string	  */
	const char	*par2;	/* Parenthesis at end of string		  */
	int	 expctr;	/* An incremental counter:+1 for each exp */
	int	 blkid;		/* Which block are we in?		  */
};

/*
 * Structure for scheme keywords. 
 */
#define NUMSCHKEYS	4
#define MAXLENSCHKEYS	17	/* 17 = longest keyword (16)  + 1 */

char scharkey[NUMSCHKEYS][MAXLENSCHKEYS] =
	{ 
		"define",
	  	"list",
	  	"if",
	  	"lambda"
	};

static const char 	 lp = '(';
static const char 	 rp = ')';
static char 		*defnam = NULL;
static int		 lnm;

/*
 * Line has a '(' as the first non-white char.
 * Do some very basic parsing of line.
 * Multi-line not supported at the moment, To do.
 */
int
foundparen(char *funstr, int llen, int lnum)
{
	const char	*lrp = NULL;
	char		*p, *begp = NULL, *endp = NULL, *prechr = NULL;
	char		*lastchr = NULL;
	int     	 i, pctr, expctr, blkid, inquote, esc;
	int		 ret = FALSE;
	int		 elen, spc, ns;

	pctr = expctr = inquote = esc = elen = spc = ns = 0;
	blkid = 1;
	lnm = lnum;

	/*
	 * load expressions into a list called 'expentry', to be processd
	 * when all are obtained.
	 * Not really live code at the moment. Just part of the process of
	 * working out what needs to be done.
	 */
	TAILQ_INIT(&ehead);

	/*
	 * Check for blocks of code with opening and closing ().
	 * One block = (cmd p a r a m)
	 * Two blocks = (cmd p a r a m s)(hola)
	 * Two blocks = (cmd p a r (list a m s))(hola)
	 * Only single line at moment, but more for multiline.
	 */
	p = funstr;

	for (i = 0; i < llen; ++i, p++) {
		if (pctr == 0 && *p != ' ' && *p != '\t' && *p != '(') {
			if (*p == ')')
				return(dobeep_num("Extra ')' found on line:",
				    lnm));
			return(dobeep_num("Error line:", lnm));
		}
		if (begp != NULL)
			elen++;

		if (*p == '\\') {
			esc = 1;
		} else if (*p == '(') {
			if (lastchr != NULL && *lastchr == '(')
				return(dobeep_num("Multiple consecutive "\
				    "left parentheses line", lnm));
			if (inquote == 0) {
				if (begp != NULL) {
					if (*prechr == ' ')
						ns--;
					if (endp == NULL)
						*p = '\0';
					else
						*endp = '\0';

					ret = parse(begp, lrp, &lp, blkid,
					    ++expctr, elen - spc, ns);
					if (!ret) {
						cleanup();
						return(ret);
					}
					elen = 0;
				}
				lrp = &lp;
				begp = endp = NULL;
				pctr++;
			} else if (inquote != 1) {
				cleanup();
				return(dobeep_num("Opening and closing quote "\
				    "char error line:", lnm));
			}
			esc = spc = 0;
		} else if (*p == ')') {
			if (lastchr != NULL && *lastchr == '(')
				return(dobeep_num("Empty parenthesis "\
				    "not supported line", lnm));
			if (inquote == 0) {
				if (begp != NULL) {
					if (*prechr == ' ')
						ns--;
					if (endp == NULL)
						*p = '\0';
					else
						*endp = '\0';

					ret = parse(begp, lrp, &rp, blkid,
					    ++expctr, elen - spc, ns);
					if (!ret) {
						cleanup();
						return(ret);
					}
					elen = 0;
				}
				lrp = &rp;
				begp = endp = NULL;
				pctr--;
			} else if (inquote != 1) {
				cleanup();
				return(dobeep_num("Opening and closing quote "\
				    "char error line:", lnm));
			}
			esc = spc = 0;
		} else if (*p != ' ' && *p != '\t') {
			if (begp == NULL) {
				begp = p;
				if (*begp == '"' || isdigit((short)*begp))
					return(dobeep_num("First char of "\
					    "expression error line:", lnm));
			}
			if (*p == '"') {
				if (inquote == 0 && esc == 0) {
					if (*prechr != ' ' && *prechr != '\t')
						return(dobeep_num("Parse error"\
						    " line:", lnm));
					inquote++;
				} else if (inquote > 0 && esc == 1)
					esc = 0;
				else
					inquote--;
			} else if (*prechr == '"' && inquote == 0) {
				return(dobeep_num("Parse error line:", lnm));
			}
			endp = NULL;
			spc = 0;
		} else if (endp == NULL && (*p == ' ' || *p == '\t')) {
			if (inquote == 0) {
				*p = ' ';
				endp = p;
				spc++;
				if (begp != NULL)
					ns++;
			}
			esc = 0;
		} else if (*p == '\t' || *p == ' ') {
			if (inquote == 0) {
				*p = ' ';
				spc++;
			}
			esc = 0;
		}
		if (*p != '\t' && *p != ' ' && inquote == 0)
			lastchr = p;

		if (pctr == 0) {
			blkid++;
			expctr = 0;
			defnam = NULL;
		}
		prechr = p;
	}

	if (pctr != 0) {
		cleanup();
		return(dobeep_num("Opening and closing parentheses error line:",
		    lnm));
	}
	if (ret == FALSE)
		cleanup();
	else
		clearexp();	/* leave lists but remove expressions */

	return (ret);
}


static int
parse(char *begp, const char *par1, const char *par2, int blkid, int expctr,
    int elen, int ns)
{
	char    *regs;
	int 	 ret = FALSE;

	if (strncmp(begp, "define", 6) == 0) {
		ret = parsdef(begp, par1, par2, blkid, expctr, elen);
		if (ret == TRUE || ret == FALSE)
			return (ret);
	} else if (strncmp(begp, "list", 4) == 0)
		return(parsval(begp, par1, par2, blkid, expctr, elen));

	regs = "^exit$";
	if (doregex(regs, begp))
		return(exitinterpreter(NULL, NULL, FALSE));

	/* mg function name regex */	
	regs = "^[A-Za-z-]+$";
        if (doregex(regs, begp))
		return(excline(begp, 0, 0));

	/* Corner case 1 */
	if (strncmp(begp, "global-set-key ", 15) == 0)
		/* function name as 2nd param screws up multiarg. */
		return(excline(begp, 0, 0));

	/* Corner case 2 */
	if (strncmp(begp, "define-key ", 11) == 0)
		/* function name as 3rd param screws up multiarg. */
		return(excline(begp, 0, 0));

	return (parsexp(begp, par1, par2, blkid, expctr, elen));
}

static int
parsdef(char *begp, const char *par1, const char *par2, int blkid, int expctr,
    int elen)
{
	char    *regs;

	if ((defnam == NULL) && (expctr != 1))
		return(dobeep_num("'define' incorrectly used line:", lnm));

        /* Does the line have a incorrect variable 'define' like: */
        /* (define i y z) */
        regs = "^define[ ]+[A-Za-z][.0-9_A-Z+a-z-]*[ ]+.+[ ]+.+$";
        if (doregex(regs, begp))
                return(dobeep_num("Invalid use of define line:", lnm));

        /* Does the line have a single variable 'define' like: */
        /* (define i 0) */
        regs = "^define[ ]+[A-Za-z][.0-9_A-Z+a-z-]*[ ]+.*$";
        if (doregex(regs, begp)) {
		if (par1 == &lp && par2 == &rp && expctr == 1)
			return(founddef(begp, blkid, expctr, 1, elen));
		return(dobeep_num("Invalid use of define line:", lnm));
	}
	/* Does the line have  '(define i(' */
        regs = "^define[ ]+[A-Za-z][.0-9_A-Z+a-z-]*[ ]*$";
        if (doregex(regs, begp)) {
		if (par1 == &lp && par2 == &lp && expctr == 1)
                	return(founddef(begp, blkid, expctr, 0, elen));
		return(dobeep_num("Invalid use of 'define' line:", lnm));
	}
	/* Does the line have  '(define (' */
	regs = "^define$";
	if (doregex(regs, begp)) {
		if (par1 == &lp && par2 == &lp && expctr == 1)
			return(foundfun(begp, expctr));
		return(dobeep_num("Invalid use of 'define' line:", lnm));
	}

	return (ABORT);
}

static int
parsval(char *begp, const char *par1, const char *par2, int blkid, int expctr,
    int elen)
{
	char    *regs;

	/* Does the line have 'list' */
	regs = "^list$";
	if (doregex(regs, begp))
		return(dobeep_num("Invalid use of list line:", lnm));

        /* Does the line have a 'list' like: */
        /* (list "a" "b") */
        regs = "^list[ ]+.*$";
        if (doregex(regs, begp)) {
		if (expctr == 1)
			return(dobeep_num("list with no-where to go.", lnm));

		if (par1 == &lp && expctr > 1)
			return(foundlst(begp, blkid, expctr, elen));

		return(dobeep_num("Invalid use of list line:", lnm));
	}
	return (FALSE);
}

static int
parsexp(char *begp, const char *par1, const char *par2, int blkid, int expctr,
    int elen)
{
	struct expentry *e1 = NULL;
	PF		 funcp;
	char		*cmdp, *fendp, *valp, *fname, *funb = NULL;;
	int		 numparams, ret;

	cmdp = begp;
	fendp = strchr(cmdp, ' ');
	if (!fendp)
		return (dobeep_msgs("Failed parsing expression, missing ' ': ", cmdp));
	*fendp = '\0';

	/*
	 * If no extant mg command found, just return.
	 */
	if ((funcp = name_function(cmdp)) == NULL)
		return (dobeep_msgs("Unknown command:", cmdp));

	numparams = numparams_function(funcp);
	if (numparams == 0)
		return (dobeep_msgs("Command takes no arguments:", cmdp));

	if (numparams == -1)
		return (dobeep_msgs("Interactive command found:", cmdp));

	if ((e1 = malloc(sizeof(struct expentry))) == NULL) {
		cleanup();
		return (dobeep_msg("malloc Error"));
	}
	TAILQ_INSERT_HEAD(&ehead, e1, eentry);
	if ((e1->fun = strndup(cmdp, BUFSIZE)) == NULL) {
		cleanup();
		return(dobeep_msg("strndup error"));
	}
	cmdp = e1->fun;
	fname = e1->fun;
	e1->funbuf[0] = '\0';
	funb = e1->funbuf;
	e1->expctr = expctr;
	e1->blkid = blkid;
	/* need to think about these two */
	e1->par1 = par1;
	e1->par2 = par2;

	*fendp = ' ';
	valp = fendp + 1;

	ret = expandvals(cmdp, valp, funb);
	if (!ret)
		return (ret);

	return (multiarg(fname, funb, numparams));
}

/*
 * Pass a list of arguments to a function.
 */
static int
multiarg(char *cmdp, char *argbuf, int numparams)
{
	char	 excbuf[BUFSIZE];
	char	*argp, *p, *s = " ";
	char	*regs;
	int	 spc, numspc;
	int	 fin, inquote;

	argp = argbuf;
	spc = 1; /* initially fake a space so we find first argument */
	numspc = fin = inquote = 0;

	for (p = argbuf; *p != '\0'; p++) {
		if (*(p + 1) == '\0')
			fin = 1;

		if (*p != ' ') {
			if (*p == '"') {
				if (inquote == 1)
					inquote = 0;	
				else
					inquote = 1;
			}
			if (spc == 1)
				if ((numspc % numparams) == 0) {
					argp = p;
				}
			spc = 0;
		}
		if ((*p == ' ' && inquote == 0) || fin) {
			if (spc == 1)/* || (numspc % numparams == 0))*/
				continue;
			if ((numspc % numparams) != (numparams - 1)) {
				numspc++;
				continue;
			}
			if (*p == ' ') {
				*p = '\0';		/* terminate arg string */
			}
			excbuf[0] = '\0';
			regs = "[\"]+.*[\"]+";

       			if (!doregex(regs, argp)) {
				const char *errstr;

				strtonum(argp, 0, INT_MAX, &errstr);
				if (errstr != NULL)
					return (dobeep_msgs("Var not found:",
					    argp));
			}

			if (strlcpy(excbuf, cmdp, sizeof(excbuf))
			    >= sizeof(excbuf))
				return (dobeep_msg("strlcpy error"));
			if (strlcat(excbuf, s, sizeof(excbuf))
			    >= sizeof(excbuf))
				return (dobeep_msg("strlcat error"));
			if (strlcat(excbuf, argp, sizeof(excbuf))
			    >= sizeof(excbuf))
				return (dobeep_msg("strlcat error"));

			excline(excbuf, 0, 0);

			if (fin)
				break;

			*p = ' ';		/* unterminate arg string */
			numspc++;
			spc = 1;
		}
	}
	return (TRUE);
}

/*
 * Is an item a value or a variable?
 */
static int
isvar(char **argp, char **varbuf, int sizof)
{
	struct varentry *v1 = NULL;

	if (SLIST_EMPTY(&varhead))
		return (FALSE);
#ifdef  MGLOG
	mglog_isvar(*varbuf, *argp, sizof);
#endif
	SLIST_FOREACH(v1, &varhead, entry) {
		if (strcmp(*argp, v1->v_name) == 0) {
			(void)((int)strlcpy(*varbuf, v1->v_buf, sizof) >= sizof);
			return (TRUE);
		}
	}
	return (FALSE);
}


static int
foundfun(char *defstr, int expctr)
{
	return (TRUE);
}

static int
foundlst(char *defstr, int blkid, int expctr, int elen)
{
	char		*p;

	p = strstr(defstr, " ");
	p = skipwhite(p);
	expandvals(NULL, p, defnam);

	return (TRUE);
}

/*
 * 'define' strings follow the regex in parsdef().
 */
static int
founddef(char *defstr, int blkid, int expctr, int hasval, int elen)
{
	struct varentry *vt, *v1 = NULL;
	char		*p, *vnamep, *vendp = NULL, *valp;

	p = strstr(defstr, " ");        /* move to first ' ' char.    */
	vnamep = skipwhite(p);		/* find first char of var name. */
	vendp = vnamep;

	/* now find the end of the define/list name */
	while (1) {
		++vendp;
		if (*vendp == ' ')
			break;
	}
	*vendp = '\0';

	/*
	 * Check list name is not an existing mg function.
	 */
	if (name_function(vnamep) != NULL)
		return(dobeep_msgs("Variable/function name clash:", vnamep));

	if (!SLIST_EMPTY(&varhead)) {
		SLIST_FOREACH_SAFE(v1, &varhead, entry, vt) {
			if (strcmp(vnamep, v1->v_name) == 0)
				SLIST_REMOVE(&varhead, v1, varentry, entry);
		}
	}
	if ((v1 = malloc(sizeof(struct varentry))) == NULL)
		return (ABORT);
	SLIST_INSERT_HEAD(&varhead, v1, entry);
	if ((v1->v_name = strndup(vnamep, BUFSIZE)) == NULL)
		return(dobeep_msg("strndup error"));
	vnamep = v1->v_name;
	v1->v_count = 0;
	v1->v_vals = NULL;
	v1->v_buf[0] = '\0';

	defnam = v1->v_buf;

	if (hasval) {
		valp = skipwhite(vendp + 1);

		expandvals(NULL, valp, defnam);
		defnam = NULL;
	}
	*vendp = ' ';	
	return (TRUE);
}


static int
expandvals(char *cmdp, char *valp, char *bp)
{
	char	 argbuf[BUFSIZE], contbuf[BUFSIZE], varbuf[BUFSIZE];
	char	*argp, *regs, *endp, *p, *v, *s = " ";
	int	 spc, sizof, fin, inquote;

	/* now find the first argument */
	p = skipwhite(valp);

	if (strlcpy(argbuf, p, sizeof(argbuf)) >= sizeof(argbuf))
		return (dobeep_msg("strlcpy error"));
	argp = argbuf;
	spc = 1; /* initially fake a space so we find first argument */
	fin = inquote = spc = 0;

	for (p = argbuf; *p != '\0'; p++) {
		if (*(p + 1) == '\0')
			fin = 1;

		if (*p != ' ') {
			if (*p == '"') {
				if (inquote == 1)
					inquote = 0;	
				else
					inquote = 1;
			}
			if (spc == 1)
				argp = p;
			spc = 0;
		}
		if ((*p == ' ' && inquote == 0) || fin) {
			if (spc == 1)
				continue;
			/* terminate arg string */
			if (*p == ' ') {
				*p = '\0';		
			}
			endp = p + 1;
			varbuf[0] = '\0';
			contbuf[0] = '\0';			
			sizof = sizeof(varbuf);
			v = varbuf;
			regs = "[\"]+.*[\"]+";
       			if (doregex(regs, argp))
				;			/* found quotes */
			else if (isvar(&argp, &v, sizof)) {

				(void)((int)strlcat(varbuf, " ",
                                    sizof) >= sizof);

				*p = ' ';
				(void)(strlcpy(contbuf, endp,
				    sizeof(contbuf)) >= sizeof(contbuf));

				(void)((int)strlcat(varbuf, contbuf,
				    sizof) >= sizof);
				
				argbuf[0] = ' ';
				argbuf[1] = '\0';
				(void)((int)strlcat(argbuf, varbuf,
				    sizof) >= sizof);

				p = argp = argbuf;
				spc = 1;
				fin = 0;
				continue;
			} else {
				const char *errstr;

				strtonum(argp, 0, INT_MAX, &errstr);
				if (errstr != NULL)
					return (dobeep_msgs("Var not found:",
					    argp));
			}
#ifdef  MGLOG
        mglog_misc("x|%s|%p|%d|\n", bp, defnam, BUFSIZE);
#endif
			if (*bp != '\0') {
				if (strlcat(bp, s, BUFSIZE) >= BUFSIZE)
					return (dobeep_msg("strlcat error"));
			}
			if (strlcat(bp, argp, BUFSIZE) >= BUFSIZE) {
				return (dobeep_msg("strlcat error"));
			}
/*			v1->v_count++;*/
			
			if (fin)
				break;

			*p = ' ';		/* unterminate arg string */
			spc = 1;
		}
	}
	return (TRUE);
}

/*
 * Finished with buffer evaluation, so clean up any vars.
 * Perhaps keeps them in mg even after use,...
 */
/*static int
clearvars(void)
{
	struct varentry	*v1 = NULL;

	while (!SLIST_EMPTY(&varhead)) {
		v1 = SLIST_FIRST(&varhead);
		SLIST_REMOVE_HEAD(&varhead, entry);
		free(v1->v_name);
		free(v1);
	}
	return (FALSE);
}
*/
/*
 * Finished with block evaluation, so clean up any expressions.
 */
static void
clearexp(void)
{
	struct expentry	*e1 = NULL;

	while (!TAILQ_EMPTY(&ehead)) {
		e1 = TAILQ_FIRST(&ehead);
		TAILQ_REMOVE(&ehead, e1, eentry);
		free(e1->fun);
		free(e1);
	}
	return;
}

/*
 * Cleanup before leaving.
 */
void
cleanup(void)
{
	defnam = NULL;

	clearexp();
/*	clearvars();*/
}

/*
 * Test a string against a regular expression.
 */
static int
doregex(char *r, char *e)
{
	regex_t  regex_buff;

	if (regcomp(&regex_buff, r, REG_EXTENDED)) {
		regfree(&regex_buff);
		return(dobeep_num("Regex compilation error line:", lnm));
	}
	if (!regexec(&regex_buff, e, 0, NULL, 0)) {
		regfree(&regex_buff);
		return(TRUE);
	}
	regfree(&regex_buff);
	return(FALSE);
}

/*
 * Display a message so it is apparent that this is the method which stopped
 * execution.
 */
static int
exitinterpreter(char *ptr, char *dobuf, int dosiz)
{
	cleanup();
	if (batch == 0)
		return(dobeep_msg("Interpreter exited via exit command."));
	return(FALSE);
}

/*
 * All code below commented out (until end of file).
 *
 * Need to think about how interpreter functions are done.
 * Probably don't have a choice with string-append().

static int 	 getenvironmentvariable(char *, char *, int);
static int	 stringappend(char *, char *, int);

typedef int	 (*PFI)(char *, char *, int);


struct ifunmap {
	PFI		 fn_funct;
	const char 	*fn_name;
	struct ifunmap	*fn_next;
};
static struct ifunmap *ifuns;

static struct ifunmap ifunctnames[] = {
	{exitinterpreter, "exit"},
	{getenvironmentvariable, "get-environment-variable"},
	{stringappend, "string-append"},
	{NULL, NULL}
};

void
ifunmap_init(void)
{
	struct ifunmap *fn;

	for (fn = ifunctnames; fn->fn_name != NULL; fn++) {
		fn->fn_next = ifuns;
		ifuns = fn;
	}
}

PFI
name_ifun(const char *ifname)
{
	struct ifunmap 	*fn;

	for (fn = ifuns; fn != NULL; fn = fn->fn_next) {
		if (strcmp(fn->fn_name, ifname) == 0)
			return (fn->fn_funct);
	}

	return (NULL);
}


int
dofunc(char **ifname, char **tmpbuf, int sizof)
{
	PFI 	 fnc;
	char	*p, *tmp;

	p = strstr(*ifname, " ");
	*p = '\0';

	fnc = name_ifun(*ifname);
	if (fnc == NULL)
		return (FALSE);

	*p = ' ';

	tmp = *tmpbuf;

	fnc(p, tmp, sizof);

	return (TRUE);
}

static int
getenvironmentvariable(char *ptr, char *dobuf, int dosiz)
{
	char		*t;
	char		*tmp;
	const char	*q = "\"";

	t = skipwhite(ptr);

	if (t[0] == *q || t[strlen(t) - 1] == *q)
		return (dobeep_msgs("Please remove '\"' around:", t));
	if ((tmp = getenv(t)) == NULL || *tmp == '\0')
		return(dobeep_msgs("Envar not found:", t));

	dobuf[0] = '\0';
	if (strlcat(dobuf, q, dosiz) >= dosiz)
		return (dobeep_msg("strlcat error"));
	if (strlcat(dobuf, tmp, dosiz) >= dosiz)
		return (dobeep_msg("strlcat error"));
	if (strlcat(dobuf, q, dosiz) >= dosiz)
		return (dobeep_msg("strlcat error"));
		
	return (TRUE);
}

static int
stringappend(char *ptr, char *dobuf, int dosiz)
{
	char		 varbuf[BUFSIZE], funbuf[BUFSIZE];
	char            *p, *f, *v, *vendp;
	int		 sizof, fin = 0;

	varbuf[0] = funbuf[0] = '\0';
	f = funbuf;
	v = varbuf;
	sizof = sizeof(varbuf);
	*dobuf = '\0';

	p = skipwhite(ptr);

	while (*p != '\0') {
		vendp = p;
		while (1) {
			if (*vendp == ' ') {
				break;
			} else if (*vendp == '\0') {
				fin = 1;
				break;
			}
			++vendp;
		}
        	*vendp = '\0';

		if (isvar(&p, &v, sizof)) {
			if (v[0] == '"' && v[strlen(v) - 1] == '"' ) {
				v[strlen(v) - 1] = '\0';
				v = v + 1;
			}
			if (strlcat(f, v, sizof) >= sizof)
				return (dobeep_msg("strlcat error"));		
		} else {
			if (p[0] == '"' && p[strlen(p) - 1] == '"' ) {
				p[strlen(p) - 1] = '\0';
				p = p + 1;
			}
			if (strlcat(f, p, sizof) >= sizof)
				return (dobeep_msg("strlcat error"));		
		}
		if (fin)
			break;
		vendp++;
		if (*vendp == '\0')
			break;
		p = skipwhite(vendp);
	}

	(void)snprintf(dobuf, dosiz, "\"%s\"", f);

	return (TRUE);
}

Index: main.c
===================================================================
RCS file: /cvs/src/usr.bin/mg/main.c,v
retrieving revision 1.89
diff -u -p -u -p -r1.89 main.c
--- main.c      20 Mar 2021 09:00:49 -0000      1.89
+++ main.c      12 Apr 2021 17:58:52 -0000
@@ -133,10 +133,12 @@ main(int argc, char **argv)
                extern void grep_init(void);
                extern void cmode_init(void);
                extern void dired_init(void);
+               extern void ifunmap_init(void);

                dired_init();
                grep_init();
                cmode_init();
+               ifunmap_init();
        }


*/
