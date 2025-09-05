/*	$OpenBSD: def.h,v 1.180 2023/04/21 13:39:37 op Exp $	*/

/* This file is in the public domain. */

/*
 * This file is the general header file for all parts
 * of the Mg display editor. It contains all of the
 * general definitions and macros. It also contains some
 * conditional compilation flags. All of the per-system and
 * per-terminal definitions are in special header files.
 */

#include	"config.h"

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
# include	<sys/queue.h>
# include	<sys/tree.h>
# ifdef __FreeBSD__
#  include	<libutil.h>
# else
#  include	<util.h>
# endif
#else /* __NotBSD__, DragonFly BSD, or macOS */
# include	"queue.h"
# include	"tree.h"
# if defined(__DragonFly__) || defined(__APPLE__)
#  include	<util.h>
# endif
#endif

#ifdef HAVE_PTY_H
# include	<pty.h>			/* openpty() et al. */
#endif

#ifdef HAVE_UTMP_H
# include	<utmp.h>		/* login_pty() on Linux */
#endif

#ifdef __sun
#include	<strings.h>
#include	<unistd.h>
#endif

#include	<signal.h>
#include	<sys/stat.h>
#include	<termios.h>		/* struct winsize et al. */

#ifndef HAVE_STRLCPY
size_t  strlcpy(char *dst, const char *src, size_t siz);
#endif

#ifndef HAVE_STRLCAT
size_t  strlcat(char *dst, const char *src, size_t siz);
#endif

#ifndef HAVE_STRTONUM
long long strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp);
#endif

#ifndef HAVE_REALLOCARRAY
void   *reallocarray(void *optr, size_t nmemb, size_t size);
#endif

#ifndef HAVE_FPARSELN
char   *fparseln(FILE *, size_t *, size_t *, const char[3], int);
#define FPARSELN_UNESCESC	0x01
#define FPARSELN_UNESCCONT	0x02
#define FPARSELN_UNESCCOMM	0x04
#define FPARSELN_UNESCREST	0x08
#define FPARSELN_UNESCALL	0x0f
#endif

#ifndef HAVE_LOGIN_TTY
int	login_tty(int fd);
#endif

#ifndef HAVE_OPENPTY
int	openpty(int *amaster, int *aslave, char *name, struct termios *termp, struct winsize *winp);
#endif

#ifndef HAVE_FUTIMENS
int     futimens(int, const struct timespec[2]);
#endif

#ifndef __dead
#define __dead
#endif

#if defined(__APPLE__) || defined(__NetBSD__) 
#define st_atim st_atimespec
#define st_ctim st_ctimespec
#define st_mtim st_mtimespec
#endif

#ifndef LOGIN_NAME_MAX
# ifdef _POSIX_LOGIN_NAME_MAX
#  define LOGIN_NAME_MAX _POSIX_LOGIN_NAME_MAX
# else
#  define LOGIN_NAME_MAX 9
# endif
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL	SO_NOSIGPIPE
#endif

#include	"chrdef.h"

typedef int	(*PF)(int, int);	/* generally useful type */

/*
 * Table sizes, etc.
 */
#define NFILEN	1024		/* Length, file name.		 */
#define NBUFN	NFILEN		/* Length, buffer name.		 */
#define NLINE	256		/* Length, line.		 */
#define PBMODES 4		/* modes per buffer		 */
#define NPAT	80		/* Length, pattern.		 */
#define HUGE	1000		/* A rather large number.	 */
#define NSRCH	128		/* Undoable search commands.	 */
#define NXNAME	64		/* Length, extended command.	 */
#define NKNAME	20		/* Length, key names.		 */
#define NTIME	50		/* Length, timestamp string.	 */

/*
 * Universal.
 */
#define FALSE	0		/* False, no, bad, etc.		 */
#define TRUE	1		/* True, yes, good, etc.	 */
#define ABORT	2		/* Death, ^G, abort, etc.	 */
#define UERROR	3		/* User Error.			 */
#define REVERT	4		/* Revert the buffer		 */

#define KCLEAR	2		/* clear echo area		 */

/*
 * These flag bits keep track of
 * some aspects of the last command. The CFCPCN
 * flag controls goal column setting. The CFKILL
 * flag controls the clearing versus appending
 * of data in the kill buffer.
 */
#define CFCPCN	0x0001		/* Last command was C-p or C-n	 */
#define CFKILL	0x0002		/* Last command was a kill	 */
#define CFINS	0x0004		/* Last command was self-insert	 */

/*
 * File I/O.
 */
#define FIOSUC	0		/* Success.			 */
#define FIOFNF	1		/* File not found.		 */
#define FIOEOF	2		/* End of file.			 */
#define FIOERR	3		/* Error.			 */
#define FIOLONG 4		/* long line partially read	 */
#define FIODIR  5		/* File is a directory		 */
#define FIOGZIP 6		/* File is gzipped (read-only)   */

/*
 * Display colors.
 */
#define CNONE	0		/* Unknown color.		 */
#define CTEXT	1		/* Text color.			 */
#define CMODE	2		/* Mode line color.		 */

/*
 * Flags for keyboard invoked functions.
 */
#define FFUNIV		1	/* universal argument		 */
#define FFNEGARG	2	/* negative only argument	 */
#define FFOTHARG	4	/* other argument		 */
#define FFARG		7	/* any argument			 */
#define FFRAND		8	/* Called by other function	 */

/*
 * Flags for "eread".
 */
#define EFFUNC	0x0001		/* Autocomplete functions.	 */
#define EFBUF	0x0002		/* Autocomplete buffers.	 */
#define EFFILE	0x0004		/* " files (maybe someday)	 */
#define EFAUTO	0x0007		/* Some autocompletion on	 */
#define EFNEW	0x0008		/* New prompt.			 */
#define EFCR	0x0010		/* Echo CR at end; last read.	 */
#define EFDEF	0x0020		/* buffer contains default args	 */
#define EFNUL	0x0040		/* Null Minibuffer OK		 */

/*
 * Direction of insert into kill ring
 */
#define KNONE	0x00
#define KFORW	0x01		/* forward insert into kill ring */
#define KBACK	0x02		/* Backwards insert into kill ring */
#define KREG	0x04		/* This is a region-based kill */

#define MAX_TOKEN 64

#define	BUFSIZE	128	/* Size of line contents in extend.c  */
/*
 * Previously from sysdef.h
 */
typedef int	RSIZE;		/* Type for file/region sizes    */
typedef short	KCHAR;		/* Type for internal keystrokes  */

/*
 * This structure holds the starting position
 * (as a line/offset pair) and the number of characters in a
 * region of a buffer. This makes passing the specification
 * of a region around a little bit easier.
 */
struct region {
	struct line	*r_linep;	/* Origin line address.		 */
	int		 r_offset;	/* Origin line offset.		 */
	int		 r_lineno;	/* Origin line number		 */
	RSIZE		 r_size;	/* Length in characters.	 */
};


/*
 * All text is kept in circularly linked
 * lists of "line" structures. These begin at the
 * header line (which is the blank line beyond the
 * end of the buffer). This line is pointed to by
 * the "buffer" structure. Each line contains the number of
 * bytes in the line (the "used" size), the size
 * of the text array, and the text. The end of line
 * is not stored as a byte; it's implied. Future
 * additions will include update hints, and a
 * list of marks into the line.
 */
struct line {
	struct line	*l_fp;		/* Link to the next line	 */
	struct line	*l_bp;		/* Link to the previous line	 */
	int		 l_size;	/* Allocated size		 */
	int		 l_used;	/* Used size			 */
	char		*l_text;	/* Content of the line		 */
};

/*
 * The rationale behind these macros is that you
 * could (with some editing, like changing the type of a line
 * link from a "struct line *" to a "REFLINE", and fixing the commands
 * like file reading that break the rules) change the actual
 * storage representation of lines to use something fancy on
 * machines with small address spaces.
 */
#define lforw(lp)	((lp)->l_fp)
#define lback(lp)	((lp)->l_bp)
#define lgetc(lp, n)	(CHARMASK((lp)->l_text[(n)]))
#define lputc(lp, n, c) ((lp)->l_text[(n)]=(c))
#define llength(lp)	((lp)->l_used)
#define ltext(lp)	((lp)->l_text)

/*
 * All repeated structures are kept as linked lists of structures.
 * All of these start with a LIST structure (except lines, which
 * have their own abstraction). This will allow for
 * later conversion to generic list manipulation routines should
 * I decide to do that. It does mean that there are four extra
 * bytes per window. I feel that this is an acceptable price,
 * considering that there are usually only one or two windows.
 */
struct list {
	union {
		struct mgwin	*l_wp;
		struct buffer	*x_bp;	/* l_bp is used by struct line */
		struct list	*l_nxt;
	} l_p;
	char *l_name;
};

/*
 * Usual hack - to keep from uglifying the code with lotsa
 * references through the union, we #define something for it.
 */
#define l_next	l_p.l_nxt

/*
 * There is a window structure allocated for
 * every active display window. The windows are kept in a
 * big list, in top to bottom screen order, with the listhead at
 * "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands
 * to guide redisplay; although this is a bit of a compromise in
 * terms of decoupling, the full blown redisplay is just too
 * expensive to run for every input character.
 */
struct mgwin {
	struct list	 w_list;	/* List header			*/
	struct buffer	*w_bufp;	/* Buffer displayed in window	*/
	struct line	*w_linep;	/* Top line in the window	*/
	struct line	*w_dotp;	/* Line containing "."		*/
	struct line	*w_markp;	/* Line containing "mark"	*/
	int		 w_doto;	/* Byte offset for "."		*/
	int		 w_marko;	/* Byte offset for "mark"	*/
	int		 w_toprow;	/* Origin 0 top row of window	*/
	int		 w_ntrows;	/* # of rows of text in window	*/
	int		 w_frame;	/* #lines to reframe by.	*/
	char		 w_rflag;	/* Redisplay Flags.		*/
	char		 w_flag;	/* Flags.			*/
	struct line	*w_wrapline;
	int		 w_dotline;	/* current line number of dot	*/
	int		 w_markline;	/* current line number of mark	*/
};
#define w_wndp	w_list.l_p.l_wp
#define w_name	w_list.l_name

/*
 * Window redisplay flags are set by command processors to
 * tell the display system what has happened to the buffer
 * mapped by the window. Setting "WFFULL" is always a safe thing
 * to do, but it may do more work than is necessary. Always try
 * to set the simplest action that achieves the required update.
 * Because commands set bits in the "w_flag", update will see
 * all change flags, and do the most general one.
 */
#define WFFRAME 0x01			/* Force reframe.		 */
#define WFMOVE	0x02			/* Movement from line to line.	 */
#define WFEDIT	0x04			/* Editing within a line.	 */
#define WFFULL	0x08			/* Do a full display.		 */
#define WFMODE	0x10			/* Update mode line.		 */

/*
 * Variable structure.
 */
struct varentry {
	SLIST_ENTRY(varentry) entry;
	char	 v_buf[BUFSIZE];
	char	*v_name;
	char	*v_vals;
	int	 v_count;
};
SLIST_HEAD(vhead, varentry);

/*
 * Window flags
 */
#define WNONE  0x00 			/* No special window options.	*/
#define WEPHEM 0x01 			/* Window is ephemeral.	 	*/

struct undo_rec;
TAILQ_HEAD(undoq, undo_rec);

/*
 * Previously from sysdef.h
 * Only used in struct buffer.
 */
struct fileinfo {
        uid_t           fi_uid;
        gid_t           fi_gid;
        mode_t          fi_mode;
        struct timespec fi_mtime;       /* Last modified time */
};

/*
 * Text is kept in buffers. A buffer header, described
 * below, exists for every buffer in the system. The buffers are
 * kept in a big list, so that commands that search for a buffer by
 * name can find the buffer header. There is a safe store for the
 * dot and mark in the header, but this is only valid if the buffer
 * is not being displayed (that is, if "b_nwnd" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with
 * a pointer to the header line in "b_headp".
 */
struct buffer {
	struct list	 b_list;	/* buffer list pointer		 */
	struct buffer	*b_altb;	/* Link to alternate buffer	 */
	struct line	*b_dotp;	/* Link to "." line structure	 */
	struct line	*b_markp;	/* ditto for mark		 */
	struct line	*b_headp;	/* Link to the header line	 */
	struct maps_s	*b_modes[PBMODES]; /* buffer modes		 */
	int		 b_doto;	/* Offset of "." in above line	 */
	int		 b_marko;	/* ditto for the "mark"		 */
	short		 b_nmodes;	/* number of non-fundamental modes */
	char		 b_nwnd;	/* Count of windows on buffer	 */
	char		 b_flag;	/* Flags			 */
	char		 b_fname[NFILEN]; /* File name			 */
	char		 b_cwd[NFILEN]; /* working directory		 */
	char		*b_nlseq;	/* Newline sequence of chars	 */
	char		*b_nlchr;	/* 1st newline character	 */
	int		 b_tabw;	/* Width of a tab character	 */
	struct fileinfo	 b_fi;		/* File attributes		 */
	struct undoq	 b_undo;	/* Undo actions list		 */
	struct undo_rec *b_undoptr;
	int		 b_dotline;	/* Line number of dot */
	int		 b_markline;	/* Line number of mark */
	int		 b_lines;	/* Number of lines in file	*/
};
#define b_bufp	b_list.l_p.x_bp
#define b_bname b_list.l_name

/* Some helper macros, in case they ever change to functions */
#define bfirstlp(buf)	(lforw((buf)->b_headp))
#define blastlp(buf)	(lback((buf)->b_headp))

#define BFCHG	0x01			/* Changed.			 */
#define BFBAK	0x02			/* Need to make a backup.	 */
#define BFNOTAB 0x04			/* no tab mode			 */
#define BFOVERWRITE 0x08		/* overwrite mode		 */
#define BFREADONLY  0x10		/* read only mode		 */
#define BFDIRTY     0x20		/* Buffer was modified elsewhere */
#define BFIGNDIRTY  0x40		/* Ignore modifications 	 */
#define BFDIREDDEL  0x80		/* Dired has a deleted 'D' file	 */
/*
 * This structure holds information about recent actions for the Undo command.
 */
struct undo_rec {
	TAILQ_ENTRY(undo_rec) next;
	enum {
		INSERT = 1,
		DELETE,
		BOUNDARY,
		MODIFIED,
		DELREG
	} type;
	struct region	 region;
	int		 pos;
	char		*content;
};

/*
 * Prototypes.
 */

/* tty.c X */
void		 ttinit(void);
void		 ttreinit(void);
void		 tttidy(void);
void		 ttmove(int, int);
void		 tteeol(void);
void		 tteeop(void);
void		 ttbeep(void);
void		 ttinsl(int, int, int);
void		 ttdell(int, int, int);
void		 ttwindow(int, int);
void		 ttnowindow(void);
void		 ttcolor(int);
void		 ttresize(void);

extern volatile sig_atomic_t winch_flag;

/* ttyio.c */
void		 ttopen(void);
int		 ttraw(void);
void		 ttclose(void);
int		 ttcooked(void);
int		 ttputc(int);
void		 ttflush(void);
int		 ttgetc(void);
int		 ttwait(int);
int		 charswaiting(void);

/* dir.c */
void		 dirinit(void);
int		 changedir(int, int);
int		 showcwdir(int, int);
int		 getcwdir(char *, size_t);
int		 make_dir(int, int);
int		 do_makedir(char *);
int		 ask_makedir(void);

/* dired.c */
struct buffer	*dired_(char *);
int		 dired_jump(int, int);
int 		 do_dired(char *);

/* file.c X */
int		 fileinsert(int, int);
int		 filevisit(int, int);
int		 filevisitalt(int, int);
int		 filevisitro(int, int);
int		 poptofile(int, int);
int		 readin(char *);
int		 insertfile(char *, char *, int);
int		 filewrite(int, int);
int		 filesave(int, int);
int		 buffsave(struct buffer *);
int		 makebkfile(int, int);
int		 writeout(FILE **, struct buffer *, char *);
void		 upmodes(struct buffer *);
size_t		 xbasename(char *, const char *, size_t);
int		 do_filevisitalt(char *);

/* line.c X */
struct line	*lalloc(int);
int		 lrealloc(struct line *, int);
void		 lfree(struct line *);
void		 lchange(int);
int		 linsert(int, int);
int		 lnewline_at(struct line *, int);
int		 lnewline(void);
int		 ldelete(RSIZE, int);
int		 ldelnewline(void);
int		 lreplace(RSIZE, char *);
char *		 linetostr(const struct line *);
int		 setcasereplace(int, int);

/* yank.c X */

void		 kdelete(void);
int		 kinsert(int, int);
int		 kremove(int);
int		 kchunk(char *, RSIZE, int);
int		 killline(int, int);
int		 yank(int, int);

/* window.c X */
struct mgwin	*new_window(struct buffer *);
int		 reposition(int, int);
int		 redraw(int, int);
int		 do_redraw(int, int, int);
int		 nextwind(int, int);
int		 prevwind(int, int);
int		 onlywind(int, int);
int		 splitwind(int, int);
int		 enlargewind(int, int);
int		 shrinkwind(int, int);
int		 delwind(int, int);

/* buffer.c */
int		 settabw(int, int);
int		 togglereadonly(int, int);
int		 togglereadonlyall(int, int);
struct buffer   *bfind(const char *, int);
int		 poptobuffer(int, int);
int		 killbuffer(struct buffer *);
int		 killbuffer_cmd(int, int);
int		 savebuffers(int, int);
int		 listbuffers(int, int);
int		 addlinef(struct buffer *, char *, ...);
#define	 addline(bp, text)	addlinef(bp, "%s", text)
int		 anycb(int);
int		 bclear(struct buffer *);
int		 showbuffer(struct buffer *, struct mgwin *, int);
int		 augbname(char *, const char *, size_t);
struct mgwin    *popbuf(struct buffer *, int);
int		 bufferinsert(int, int);
int		 usebuffer(int, int);
int		 notmodified(int, int);
int		 popbuftop(struct buffer *, int);
int		 getbufcwd(char *, size_t);
int		 checkdirty(struct buffer *);
int		 revertbuffer(int, int);
int		 dorevert(void);
int		 diffbuffer(int, int);
struct buffer	*findbuffer(char *);

/* display.c */
int		 vtresize(int, int, int);
void		 vtinit(void);
void		 vttidy(void);
void		 update(int);
int		 linenotoggle(int, int);
int		 colnotoggle(int, int);
int		 timetoggle(int, int);

/* echo.c X */
int		 helptoggle(int, int);
void		 eerase(void);
int		 eyorn(const char *);
int		 eynorr(const char *);
int		 eyesno(const char *);
void		 ewprintf(const char *fmt, ...);
char		*eread(const char *, char *, size_t, int, ...)
				__attribute__((__format__ (printf, 1, 5)));
int		 getxtra(struct list *, struct list *, int, int);
void		 free_file_list(struct list *);

/* fileio.c */
int		 ffropen(FILE **, const char *, struct buffer *);
void		 ffstat(FILE *, struct buffer *);
int		 ffwopen(FILE **, const char *, struct buffer *);
int		 ffclose(FILE *, struct buffer *);
int		 ffputbuf(FILE *, struct buffer *, int);
int		 ffgetline(FILE *, char *, int, int *);
int		 fbackupfile(const char *);
char		*adjustname(const char *, int);
FILE		*startupfile(char *, char *, char *, size_t);
int		 copy(char *, char *);
struct list	*make_file_list(char *);
#ifndef fisdir
int		 fisdir(const char *);
#endif
int		 fchecktime(struct buffer *);
int		 fupdstat(struct buffer *);
int		 backuptohomedir(int, int);
int		 toggleleavetmp(int, int);
char		*expandtilde(const char *);

/* kbd.c X */
int		 do_meta(int, int);
int		 bsmap(int, int);
void		 ungetkey(int);
int		 getkey(int);
int		 doin(void);
int		 rescan(int, int);
int		 universal_argument(int, int);
int		 digit_argument(int, int);
int		 negative_argument(int, int);
int		 ask_selfinsert(int, int);
int		 selfinsert(int, int);
int		 quote(int, int);

/* main.c */
int		 ctrlg(int, int);
int		 quit(int, int);

/* ttyio.c */
void		 panic(char *);

/* cinfo.c */
char		*getkeyname(char  *, size_t, int);

/* basic.c */
int		 gotobol(int, int);
int		 backchar(int, int);
int		 gotoeol(int, int);
int		 forwchar(int, int);
int		 gotobob(int, int);
int		 gotoeob(int, int);
int		 forwline(int, int);
int		 backline(int, int);
void		 setgoal(void);
int		 getgoal(struct line *);
int		 forwpage(int, int);
int		 backpage(int, int);
int		 forw1page(int, int);
int		 back1page(int, int);
int		 pagenext(int, int);
void		 isetmark(void);
int		 setmark(int, int);
int		 clearmark(int, int);
int		 swapmark(int, int);
int		 gotoline(int, int);
int		 setlineno(int);

/* util.c X */
int		 ntabstop(int, int);
int		 showcpos(int, int);
int		 getcolpos(struct mgwin *);
int		 twiddle(int, int);
int		 openline(int, int);
int		 enewline(int, int);
int		 deblank(int, int);
int		 justone(int, int);
int		 delwhite(int, int);
int		 delleadwhite(int, int);
int		 deltrailwhite(int, int);
int		 lfindent(int, int);
int		 indent(int, int);
int		 forwdel(int, int);
int		 backdel(int, int);
int		 space_to_tabstop(int, int);
int		 backtoindent(int, int);
int		 joinline(int, int);

/* tags.c X */
int		 findtag(int, int);
int 		 poptag(int, int);
int		 tagsvisit(int, int);
int		 curtoken(int, int, char *);

/* cscope.c */
int		 cssymbol(int, int);
int		 csdefinition(int, int);
int		 csfuncalled(int, int);
int		 cscallerfuncs(int, int);
int		 csfindtext(int, int);
int		 csegrep(int, int);
int		 csfindfile(int, int);
int		 csfindinc(int, int);
int		 csnextfile(int, int);
int		 csnextmatch(int, int);
int		 csprevfile(int, int);
int		 csprevmatch(int, int);
int		 cscreatelist(int, int);

/* extend.c X */
int		 insert(int, int);
int		 bindtokey(int, int);
int		 localbind(int, int);
int		 redefine_key(int, int);
int		 unbindtokey(int, int);
int		 localunbind(int, int);
int		 extend(int, int);
int		 evalexpr(int, int);
int		 evalbuffer(int, int);
int		 evalfile(int, int);
int		 load(FILE *, const char *);
int		 excline(char *, int, int);
char		*skipwhite(char *);

/* help.c X */
extern const char	*hlp;
int		 desckey(int, int);
int		 wallchart(int, int);
int		 help_help(int, int);
int		 apropos_command(int, int);
int		 quickhelp(int, int);
int		 tutorial(int, int);

/* paragraph.c X */
int		 gotobop(int, int);
int		 gotoeop(int, int);
int		 fillpara(int, int);
int		 killpara(int, int);
int		 fillword(int, int);
int		 setfillcol(int, int);
int		 markpara(int, int);
int		 transposepara(int, int);
int		 sentencespace(int, int);

/* word.c X */
int		 backword(int, int);
int		 forwword(int, int);
int		 upperword(int, int);
int		 lowerword(int, int);
int		 capword(int, int);
int		 delfword(int, int);
int		 delbword(int, int);
int		 inword(void);
int		 transposeword(int, int);

/* region.c X */
int		 killregion(int, int);
int		 copyregion(int, int);
int		 lowerregion(int, int);
int		 upperregion(int, int);
int		 prefixregion(int, int);
int		 setprefix(int, int);
int		 region_get_data(struct region *, char *, int);
void		 region_put_data(const char *, int);
int		 markbuffer(int, int);
int		 piperegion(int, int);
int		 shellcommand(int, int);
int		 pipeio(const char * const, char * const[], char * const, int,
		     struct buffer *);

/* search.c X */
int		 forwsearch(int, int);
int		 backsearch(int, int);
int		 searchagain(int, int);
int		 forwisearch(int, int);
int		 backisearch(int, int);
int		 queryrepl(int, int);
int		 forwsrch(void);
int		 backsrch(void);
int		 readpattern(char *);
int		 zapuptochar(int, int);
int		 zaptochar(int, int);
int		 zap(int, int);

/* spawn.c X */
int		 spawncli(int, int);

/* ttykbd.c X */
void		 ttykeymapinit(void);
void		 ttykeymaptidy(void);

/* match.c X */
int		 showmatch(int, int);

/* version.c X */
int		 showversion(int, int);

/* macro.c X */
int		 definemacro(int, int);
int		 finishmacro(int, int);
int		 executemacro(int, int);
int		 endorexecmacro(int, int);
int		 applymacro(int, int);

/* modes.c X */
int		 indentmode(int, int);
int		 fillmode(int, int);
int		 notabmode(int, int);
int		 overwrite_mode(int, int);
int		 set_default_mode(int,int);

#ifdef REGEX
/* re_search.c X */
int		 re_forwsearch(int, int);
int		 re_backsearch(int, int);
int		 re_searchagain(int, int);
int		 re_queryrepl(int, int);
int		 re_repl(int, int);
int		 replstr(int, int);
int		 setcasefold(int, int);
int		 delmatchlines(int, int);
int		 delnonmatchlines(int, int);
int		 cntmatchlines(int, int);
int		 cntnonmatchlines(int, int);
#endif	/* REGEX */

/* undo.c X */
void		 free_undo_record(struct undo_rec *);
int		 undo_dump(int, int);
int		 undo_enabled(void);
int		 undo_enable(int, int);
int		 undo_add_boundary(int, int);
void		 undo_add_modified(void);
int		 undo_add_insert(struct line *, int, int);
int		 undo_add_delete(struct line *, int, int, int);
int		 undo_boundary_enable(int, int);
int		 undo_add_change(struct line *, int, int);
int		 undo(int, int);

/* autoexec.c X */
int		 auto_execute(int, int);
PF		*find_autoexec(const char *);
int		 add_autoexec(const char *, const char *);

/* cmode.c X */
int		 cmode(int, int);
int		 cc_brace(int, int);
int		 cc_char(int, int);
int		 cc_tab(int, int);
int		 cc_indent(int, int);
int		 cc_lfindent(int, int);

/* grep.c X */
#ifdef ENABLE_COMPILE_GREP
int		 next_error(int, int);
int		 globalwd(void);
int		 globalwdtoggle(int, int);
int		 compile(int, int);
#else
static inline int globalwd(void) { return FALSE; }
#endif

/* bell.c */
void		 bellinit(void);
int		 toggleaudiblebell(int, int);
int		 togglevisiblebell(int, int);
int		 dobeep_num(const char *, int);
int		 dobeep_msgs(const char *, const char *);
int		 dobeep_msg(const char *);
void		 dobeep(void);

/* interpreter.c */
int		 foundparen(char *, int, int);
void		 cleanup(void);

/*
 * Externals.
 */
extern struct buffer	*bheadp;
extern struct buffer	*curbp;
extern struct mgwin	*curwp;
extern struct mgwin	*wheadp;
extern struct vhead	 varhead;
extern int		 thisflag;
extern int		 lastflag;
extern int		 curgoal;
extern int		 startrow;
extern int		 epresf;
extern int		 sgarbf;
extern int		 mode;
extern int		 nrow;
extern int		 ncol;
extern int		 ttrow;
extern int		 ttcol;
extern int		 tttop;
extern int		 ttbot;
extern int		 tthue;
extern int		 defb_nmodes;
extern int		 defb_flag;
extern int		 doaudiblebell;
extern int		 dovisiblebell;
extern int		 dblspace;
extern int		 allbro;
extern int		 batch;
extern char	 	 cinfo[];
extern char		*keystrings[];
extern char		 pat[NPAT];
extern char		 prompt[];
extern int		 tceeol;
extern int		 tcinsl;
extern int		 tcdell;
extern int		 rptcount;	/* successive invocation count */

/*
 * Extensions.
 */
extern int		 shownlprompt;
int			 togglenewlineprompt(int, int);
