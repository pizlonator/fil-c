/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * cfdisk.c - Display or manipulate a disk partition table.
 *
 *     Copyright (C) 2014-2023 Karel Zak <kzak@redhat.com>
 *     Copyright (C) 1994 Kevin E. Martin (martin@cs.unc.edu)
 *
 *     The original cfdisk was inspired by the fdisk program
 *           by A. V. Le Blanc (leblanc@mcc.ac.uk.
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <getopt.h>
#include <assert.h>
#include <libsmartcols.h>
#include <sys/ioctl.h>
#include <rpmatch.h>
#include <libfdisk.h>

#ifdef HAVE_LIBMOUNT
# include <libmount.h>	/* keep it optional for non-linux systems */
#endif

#ifdef HAVE_SLANG_H
# include <slang.h>
#elif defined(HAVE_SLANG_SLANG_H)
# include <slang/slang.h>
#endif

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 500 /* for inclusion of get_wch */
#endif

#ifdef HAVE_SLCURSES_H
# include <slcurses.h>
#elif defined(HAVE_SLANG_SLCURSES_H)
# include <slang/slcurses.h>
#elif defined(HAVE_NCURSESW_NCURSES_H) && defined(HAVE_WIDECHAR)
# include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_H)
# include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
# include <ncurses/ncurses.h>
#endif

#ifdef HAVE_WIDECHAR
# include <wctype.h>
# include <wchar.h>
#endif

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"
#include "mbsalign.h"
#include "mbsedit.h"
#include "colors.h"
#include "debug.h"
#include "list.h"
#include "blkdev.h"

static const char *default_disks[] = {
#ifdef __GNU__
		"/dev/hd0",
		"/dev/sd0",
#elif defined(__FreeBSD__)
		"/dev/ad0",
		"/dev/da0",
#else
		"/dev/sda",
		"/dev/vda",
		"/dev/hda",
#endif
};

#define ARROW_CURSOR_STRING	">>  "
#define ARROW_CURSOR_DUMMY	"    "
#define ARROW_CURSOR_WIDTH	(sizeof(ARROW_CURSOR_STRING) - 1)

/* vertical menu */
#define MENU_V_SPADDING		1	/* space around menu item string */

/* horizontal menu */
#define MENU_H_SPADDING		0	/* space around menu item string */
#define MENU_H_BETWEEN		2	/* space between menu items */
#define MENU_H_PRESTR		"["
#define MENU_H_POSTSTR		"]"

#define MENU_TITLE_PADDING	3

#define MENU_H_PRESTR_SZ	(sizeof(MENU_H_PRESTR) - 1)
#define MENU_H_POSTSTR_SZ	(sizeof(MENU_H_POSTSTR) - 1)

#define TABLE_START_LINE	4
#define MENU_START_LINE		(ui_lines - 4)		/* The menu maybe use two lines */
#define INFO_LINE		(ui_lines - 2)
#define WARN_LINE		INFO_LINE
#define HINT_LINE		(ui_lines - 1)

#define CFDISK_ERR_ESC		5000

#ifndef KEY_ESC
# define KEY_ESC	'\033'
#endif
#ifndef KEY_DELETE
# define KEY_DELETE	'\177'
#endif
#ifndef KEY_DC
# define KEY_DC		0423
#endif


/* colors */
enum {
	CFDISK_CL_NONE = 0,
	CFDISK_CL_WARNING,
	CFDISK_CL_FREESPACE,
	CFDISK_CL_INFO
};
#ifdef HAVE_USE_DEFAULT_COLORS
static const int color_pairs[][2] = {
	/* color            foreground, background */
	[CFDISK_CL_WARNING]   = { COLOR_RED, -1 },
	[CFDISK_CL_FREESPACE] = { COLOR_GREEN, -1 },
	[CFDISK_CL_INFO]      = { COLOR_BLUE, -1 }
};
#endif

struct cfdisk;

static struct cfdisk_menuitem *menu_get_menuitem(struct cfdisk *cf, size_t idx);
static struct cfdisk_menuitem *menu_get_menuitem_by_key(struct cfdisk *cf, int key, size_t *idx);
static struct cfdisk_menu *menu_push(struct cfdisk *cf, struct cfdisk_menuitem *item);
static struct cfdisk_menu *menu_pop(struct cfdisk *cf);
static void menu_refresh_size(struct cfdisk *cf);

static int ui_end(void);
static int ui_refresh(struct cfdisk *cf);

static void ui_warnx(const char *fmt, ...)
			__attribute__((__format__ (__printf__, 1, 2)));
static void ui_warn(const char *fmt, ...)
			__attribute__((__format__ (__printf__, 1, 2)));
static void ui_info(const char *fmt, ...)
			__attribute__((__format__ (__printf__, 1, 2)));

static void ui_draw_menu(struct cfdisk *cf);
static int ui_menu_move(struct cfdisk *cf, int key);
static void ui_menu_resize(struct cfdisk *cf);

static int ui_get_size(struct cfdisk *cf, const char *prompt, uint64_t *res,
		       uint64_t low, uint64_t up, int *expsize);

static int ui_enabled;
static volatile sig_atomic_t sig_resize;
static volatile sig_atomic_t sig_die;

/* ncurses LINES and COLS may be actual variables or *macros*, but we need
 * something portable and writable */
static size_t ui_lines;
static size_t ui_cols;

/* menu item */
struct cfdisk_menuitem {
	int		key;		/* keyboard shortcut */
	const char	*name;		/* item name */
	const char	*desc;		/* item description (hint) */
	void		*userdata;
};

/* menu */
struct cfdisk_menu {
	char			*title;	/* optional menu title */
	struct cfdisk_menuitem	*items;	/* array with menu items */
	char			*ignore;/* string with keys to ignore */
	size_t			width;	/* maximal width of the menu item */
	size_t			nitems;	/* number of the active menu items */
	size_t			page_sz;/* when menu longer than screen */
	size_t			idx;	/* the current menu item */
	int			prefkey;/* preferred menu item  */
	struct cfdisk_menu	*prev;

	/* @ignore keys generator */
	int (*ignore_cb)	(struct cfdisk *, char *, size_t);

	unsigned int		vertical : 1;	/* enable vertical mode */
};

/* main menu */
static struct cfdisk_menuitem main_menuitems[] = {
	{ 'b', N_("Bootable"), N_("Toggle bootable flag of the current partition") },
	{ 'd', N_("Delete"), N_("Delete the current partition") },
	{ 'r', N_("Resize"), N_("Reduce or enlarge the current partition") },
	{ 'n', N_("New"), N_("Create new partition from free space") },
	{ 'q', N_("Quit"), N_("Quit program without writing changes") },
	{ 't', N_("Type"), N_("Change the partition type") },
	{ 'h', N_("Help"), N_("Print help screen") },
	{ 's', N_("Sort"), N_("Fix partitions order") },
	{ 'W', N_("Write"), N_("Write partition table to disk (this might destroy data)") },
	{ 'u', N_("Dump"), N_("Dump partition table to sfdisk compatible script file") },
	{ 0, NULL, NULL }
};

/* extra partinfo in name:value pairs */
struct cfdisk_extra {
	char *name;
	char *data;

	struct list_head exs;
};

/* line and extra partinfo list_head */
struct cfdisk_line {
	char			*data;		/* line data */
	struct libscols_table	*extra;		/* extra info ('X') */
	WINDOW			*w;		/* window with extra info */
};

/* top level control struct */
struct cfdisk {
	struct fdisk_context	*cxt;	/* libfdisk context */
	struct fdisk_table	*table;	/* partition table */
	struct fdisk_table	*original_layout; /* original on-disk PT */

	struct cfdisk_menu	*menu;	/* the current menu */

	int	*fields;	/* output columns IDs */
	size_t	nfields;	/* number of columns IDs */

	char	*linesbuf;	/* table as string */
	size_t	linesbufsz;	/* size of the tb_buf */

	struct	cfdisk_line	*lines;	/* list of lines */

	size_t	nlines;		/* number of lines */
	size_t	lines_idx;	/* current line <0..N>, exclude header */
	size_t  page_sz;

	unsigned int nwrites;	/* fdisk_write_disklabel() counter */

	WINDOW	*act_win;	/* the window currently on the screen */

#ifdef HAVE_LIBMOUNT
	struct libmnt_table *mtab;
	struct libmnt_table *fstab;
	struct libmnt_cache *mntcache;
#endif
	unsigned int	wrong_order :1,		/* PT not in right order */
			zero_start :1,		/* ignore existing partition table */
			device_is_used : 1,	/* don't use re-read ioctl */
			show_extra :1;		/* show extra partinfo */
};


/*
 * let's use include/debug.h stuff for cfdisk too
 */
static UL_DEBUG_DEFINE_MASK(cfdisk);
UL_DEBUG_DEFINE_MASKNAMES(cfdisk) = UL_DEBUG_EMPTY_MASKNAMES;

#define CFDISK_DEBUG_INIT	(1 << 1)
#define CFDISK_DEBUG_UI		(1 << 2)
#define CFDISK_DEBUG_MENU	(1 << 3)
#define CFDISK_DEBUG_MISC	(1 << 4)
#define CFDISK_DEBUG_TABLE	(1 << 5)
#define CFDISK_DEBUG_ALL	0xFFFF

#define DBG(m, x)       __UL_DBG(cfdisk, CFDISK_DEBUG_, m, x)

static void cfdisk_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(cfdisk, CFDISK_DEBUG_, 0, CFDISK_DEBUG);
}

/* Initialize output columns -- we follow libfdisk fields (usually specific
 * to the label type.
 */
static int cols_init(struct cfdisk *cf)
{
	assert(cf);

	free(cf->fields);
	cf->fields = NULL;
	cf->nfields = 0;

	return fdisk_label_get_fields_ids(NULL, cf->cxt, &cf->fields, &cf->nfields);
}

static void die_on_signal(void)
{
	DBG(MISC, ul_debug("die on signal."));
	ui_end();
	exit(EXIT_FAILURE);
}

static void resize(void)
{
	struct winsize ws;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &ws) != -1
	    && ws.ws_row && ws.ws_col) {
		ui_lines = ws.ws_row;
		ui_cols  = ws.ws_col;
#if HAVE_RESIZETERM
		resizeterm(ws.ws_row, ws.ws_col);
#endif
		clearok(stdscr, TRUE);
	}
	touchwin(stdscr);

	DBG(UI, ul_debug("ui: resize refresh ui_cols=%zu, ui_lines=%zu",
				ui_cols, ui_lines));
	sig_resize = 0;
}

/* Reads partition in tree-like order from scols
 */
static int partition_from_scols(struct fdisk_table *tb,
				struct libscols_line *ln)
{
	struct fdisk_partition *pa = scols_line_get_userdata(ln);

	fdisk_table_add_partition(tb, pa);
	fdisk_unref_partition(pa);

	if (scols_line_has_children(ln)) {
		struct libscols_line *chln;
		struct libscols_iter *itr = scols_new_iter(SCOLS_ITER_FORWARD);

		if (!itr)
			return -EINVAL;
		while (scols_line_next_child(ln, itr, &chln) == 0)
			partition_from_scols(tb, chln);
		scols_free_iter(itr);
	}
	return 0;
}

static char *table_to_string(struct cfdisk *cf, struct fdisk_table *tb)
{
	struct fdisk_partition *pa;
	struct fdisk_label *lb;
	struct fdisk_iter *itr;
	struct libscols_table *table = NULL;
	struct libscols_iter *s_itr = NULL;
	char *res = NULL;
	size_t i;
	int tree = 0;
	struct libscols_line *ln, *ln_cont = NULL;

	DBG(TABLE, ul_debug("convert to string"));

	assert(cf);
	assert(cf->cxt);
	assert(cf->fields);
	assert(tb);

	lb = fdisk_get_label(cf->cxt, NULL);
	assert(lb);

	itr = fdisk_new_iter(FDISK_ITER_FORWARD);
	if (!itr)
		goto done;

	/* get container (e.g. extended partition) */
	while (fdisk_table_next_partition(tb, itr, &pa) == 0) {
		if (fdisk_partition_is_nested(pa)) {
			DBG(TABLE, ul_debug("nested detected, using tree"));
			tree = SCOLS_FL_TREE;
			break;
		}
	}

	table = scols_new_table();
	if (!table)
		goto done;
	scols_table_enable_maxout(table, 1);
	scols_table_enable_nowrap(table, 1);

#if !defined(HAVE_LIBNCURSESW) || !defined(HAVE_WIDECHAR)
	scols_table_enable_ascii(table, 1);
#endif

	/* headers */
	for (i = 0; i < cf->nfields; i++) {
		int fl = 0;
		const struct fdisk_field *field =
				fdisk_label_get_field(lb, cf->fields[i]);
		if (!field)
			continue;

		if (fdisk_field_is_number(field))
			fl |= SCOLS_FL_RIGHT;
		if (fdisk_field_get_id(field) == FDISK_FIELD_TYPE)
			fl |= SCOLS_FL_TRUNC;
		if (tree && fdisk_field_get_id(field) == FDISK_FIELD_DEVICE)
			fl |= SCOLS_FL_TREE;

		if (!scols_table_new_column(table,
				_(fdisk_field_get_name(field)),
				fdisk_field_get_width(field), fl))
			goto done;
	}

	/* data */
	fdisk_reset_iter(itr, FDISK_ITER_FORWARD);

	while (fdisk_table_next_partition(tb, itr, &pa) == 0) {
		struct libscols_line *parent = fdisk_partition_is_nested(pa) ? ln_cont : NULL;

		ln = scols_table_new_line(table, parent);
		if (!ln)
			goto done;
		for (i = 0; i < cf->nfields; i++) {
			char *cdata = NULL;

			if (fdisk_partition_to_string(pa, cf->cxt,
					cf->fields[i], &cdata))
				continue;
			scols_line_refer_data(ln, i, cdata);
		}
		if (tree && fdisk_partition_is_container(pa))
			ln_cont = ln;

		scols_line_set_userdata(ln, (void *) pa);
		fdisk_ref_partition(pa);
	}

	if (scols_table_is_empty(table))
		goto done;

	scols_table_reduce_termwidth(table, ARROW_CURSOR_WIDTH);
	scols_print_table_to_string(table, &res);

	/* scols_* code might reorder lines, let's reorder @tb according to the
	 * final output (it's no problem because partitions are addressed by
	 * parno stored within struct fdisk_partition)  */

	/* remove all */
	fdisk_reset_table(tb);

	s_itr = scols_new_iter(SCOLS_ITER_FORWARD);
	if (!s_itr)
		goto done;

	/* add all in the right order (don't forget the output is tree) */
	while (scols_table_next_line(table, s_itr, &ln) == 0) {
		if (scols_line_get_parent(ln))
			continue;
		if (partition_from_scols(tb, ln))
			break;
	}
done:
	scols_unref_table(table);
	scols_free_iter(s_itr);
	fdisk_free_iter(itr);

	return res;
}

static void cfdisk_free_lines(struct cfdisk *cf)
{
	size_t i = 0;
	while(i < cf->nlines) {
		scols_unref_table(cf->lines[i].extra);

		DBG(UI, ul_debug("delete window: %p",
				cf->lines[i].w));

		if (cf->lines[i].w)
			delwin(cf->lines[i].w);
		cf->lines[i].w = NULL;
		++i;
	}
	cf->act_win = NULL;
	free(cf->lines);
	cf->lines = NULL;
}
/*
 * Read data about partitions from libfdisk and prepare output lines.
 */
static int lines_refresh(struct cfdisk *cf)
{
	int rc;
	char *p;
	size_t i;

	assert(cf);

	DBG(TABLE, ul_debug("refreshing buffer"));

	free(cf->linesbuf);
	cfdisk_free_lines(cf);
	cf->linesbuf = NULL;
	cf->linesbufsz = 0;
	cf->lines = NULL;
	cf->nlines = 0;

	fdisk_unref_table(cf->table);
	cf->table = NULL;

	/* read partitions and free spaces into cf->table */
	rc = fdisk_get_partitions(cf->cxt, &cf->table);
	if (!rc)
		rc = fdisk_get_freespaces(cf->cxt, &cf->table);
	if (rc)
		return rc;

	cf->linesbuf = table_to_string(cf, cf->table);
	if (!cf->linesbuf)
		return -ENOMEM;

	cf->linesbufsz = strlen(cf->linesbuf);
	cf->nlines = fdisk_table_get_nents(cf->table) + 1;	/* 1 for header line */
	cf->page_sz = 0;
	cf->wrong_order = fdisk_table_wrong_order(cf->table) ? 1 : 0;

	if (MENU_START_LINE - TABLE_START_LINE < cf->nlines)
		cf->page_sz = MENU_START_LINE - TABLE_START_LINE - 1;

	cf->lines = xcalloc(cf->nlines, sizeof(struct cfdisk_line));

	for (p = cf->linesbuf, i = 0; p && i < cf->nlines; i++) {
		cf->lines[i].data = p;
		p = strchr(p, '\n');
		if (p) {
			*p = '\0';
			p++;
		}
		cf->lines[i].extra = scols_new_table();
		scols_table_enable_noheadings(cf->lines[i].extra, 1);
		scols_table_new_column(cf->lines[i].extra, NULL, 0, SCOLS_FL_RIGHT);
		scols_table_new_column(cf->lines[i].extra, NULL, 0, SCOLS_FL_TRUNC);
	}

	return 0;
}

static struct fdisk_partition *get_current_partition(struct cfdisk *cf)
{
	assert(cf);
	assert(cf->table);

	return fdisk_table_get_partition(cf->table, cf->lines_idx);
}

static int is_freespace(struct cfdisk *cf, size_t i)
{
	struct fdisk_partition *pa;

	assert(cf);
	assert(cf->table);

	pa = fdisk_table_get_partition(cf->table, i);
	return fdisk_partition_is_freespace(pa);
}

/* converts libfdisk FDISK_ASKTYPE_MENU to cfdisk menu and returns user's
 * response back to libfdisk
 */
static int ask_menu(struct fdisk_ask *ask, struct cfdisk *cf)
{
	struct cfdisk_menuitem *d, *cm;
	int key;
	size_t i = 0, nitems;
	const char *name, *desc;

	assert(ask);
	assert(cf);

	/* create cfdisk menu according to libfdisk ask-menu, note that the
	 * last cm[] item has to be empty -- so nitems + 1 */
	nitems = fdisk_ask_menu_get_nitems(ask);
	cm = xcalloc(nitems + 1, sizeof(struct cfdisk_menuitem));

	for (i = 0; i < nitems; i++) {
		if (fdisk_ask_menu_get_item(ask, i, &key, &name, &desc))
			break;
		cm[i].key = key;
		cm[i].desc = desc;
		cm[i].name = name;
	}

	/* make the new menu active */
	menu_push(cf, cm);
	ui_draw_menu(cf);
	refresh();

	/* wait for keys */
	while (!sig_die) {
		key = getch();

		if (sig_die)
			break;
		if (sig_resize)
			ui_menu_resize(cf);
		if (ui_menu_move(cf, key) == 0)
			continue;

		switch (key) {
		case KEY_ENTER:
		case '\n':
		case '\r':
			d = menu_get_menuitem(cf, cf->menu->idx);
			if (d)
				fdisk_ask_menu_set_result(ask, d->key);
			menu_pop(cf);
			free(cm);
			return 0;
		}
	}

	if (sig_die)
		die_on_signal();

	menu_pop(cf);
	free(cm);
	return -1;
}

/* libfdisk callback
 */
static int ask_callback(struct fdisk_context *cxt __attribute__((__unused__)),
			struct fdisk_ask *ask,
			void *data __attribute__((__unused__)))
{
	int rc = 0;

	assert(ask);

	switch(fdisk_ask_get_type(ask)) {
	case FDISK_ASKTYPE_INFO:
		ui_info("%s", fdisk_ask_print_get_mesg(ask));
		break;
	case FDISK_ASKTYPE_WARNX:
		ui_warnx("%s", fdisk_ask_print_get_mesg(ask));
		break;
	case FDISK_ASKTYPE_WARN:
		ui_warn("%s", fdisk_ask_print_get_mesg(ask));
		break;
	case FDISK_ASKTYPE_MENU:
		ask_menu(ask, (struct cfdisk *) data);
		break;
	default:
		ui_warnx(_("internal error: unsupported dialog type %d"),
			fdisk_ask_get_type(ask));
		return -EINVAL;
	}
	return rc;
}

static int ui_end(void)
{
	if (!ui_enabled)
		return -EINVAL;

#if defined(HAVE_SLCURSES_H) || defined(HAVE_SLANG_SLCURSES_H)
	SLsmg_gotorc(ui_lines - 1, 0);
	SLsmg_refresh();
#else
	mvcur(0, ui_cols - 1, ui_lines-1, 0);
#endif
	curs_set(1);
	nl();
	endwin();
	printf("\n");
	ui_enabled = 0;
	return 0;
}

static void __attribute__((__format__ (__printf__, 3, 0)))
	ui_vprint_center(size_t line, int attrs, const char *fmt, va_list ap)
{
	size_t width;
	char *buf = NULL;

	move(line, 0);
	clrtoeol();

	xvasprintf(&buf, fmt, ap);

	width = mbs_safe_width(buf);
	if (width > (size_t) ui_cols) {
		char *p = strrchr(buf + ui_cols, ' ');
		if (!p)
			p = buf + ui_cols;
		*p = '\0';
		if (line + 1 >= ui_lines)
			line--;
		attron(attrs);
		mvaddstr(line, 0, buf);
		mvaddstr(line + 1, 0, p+1);
		attroff(attrs);
	} else {
		attron(attrs);
		mvaddstr(line, (ui_cols - width) / 2, buf);
		attroff(attrs);
	}
	free(buf);
}

static void __attribute__((__format__ (__printf__, 2, 3)))
	ui_center(size_t line, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ui_vprint_center(line, 0, fmt, ap);
	va_end(ap);
}

static void __attribute__((__format__ (__printf__, 1, 2)))
	ui_warnx(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (ui_enabled)
		ui_vprint_center(WARN_LINE,
			colors_wanted() ? COLOR_PAIR(CFDISK_CL_WARNING) : 0,
			fmt, ap);
	else {
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);
	}
	va_end(ap);
}

static void __attribute__((__format__ (__printf__, 1, 2)))
	ui_warn(const char *fmt, ...)
{
	char *fmt_m;
	va_list ap;

	xasprintf(&fmt_m, "%s: %m", fmt);

	va_start(ap, fmt);
	if (ui_enabled)
		ui_vprint_center(WARN_LINE,
			colors_wanted() ? COLOR_PAIR(CFDISK_CL_WARNING) : 0,
			fmt_m, ap);
	else {
		vfprintf(stderr, fmt_m, ap);
		fputc('\n', stderr);
	}
	va_end(ap);
	free(fmt_m);
}

static void ui_clean_warn(void)
{
	move(WARN_LINE, 0);
	clrtoeol();
}

static int __attribute__((__noreturn__))
	   __attribute__((__format__ (__printf__, 2, 3)))
	ui_err(int rc, const char *fmt, ...)
{
	va_list ap;
	ui_end();

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", program_invocation_short_name);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", strerror(errno));
	va_end(ap);

	exit(rc);
}

static int __attribute__((__noreturn__))
	   __attribute__((__format__ (__printf__, 2, 3)))
	ui_errx(int rc, const char *fmt, ...)
		{
	va_list ap;
	ui_end();

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", program_invocation_short_name);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);

	exit(rc);
}

static void __attribute__((__format__ (__printf__, 1, 2)))
	ui_info(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (ui_enabled)
		ui_vprint_center(INFO_LINE,
				colors_wanted() ? COLOR_PAIR(CFDISK_CL_INFO) : 0,
				fmt, ap);
	else {
		vfprintf(stdout, fmt, ap);
		fputc('\n', stdout);
	}
	va_end(ap);
}

static void ui_clean_info(void)
{
	move(INFO_LINE, 0);
	clrtoeol();
}

static void __attribute__((__format__ (__printf__, 1, 2)))
	ui_hint(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (ui_enabled)
		ui_vprint_center(HINT_LINE, A_BOLD, fmt, ap);
	else {
		vfprintf(stdout, fmt, ap);
		fputc('\n', stdout);
	}
	va_end(ap);
}

static void ui_clean_hint(void)
{
	move(HINT_LINE, 0);
	clrtoeol();
}


static void sig_handler_die(int dummy __attribute__((__unused__)))
{
	sig_die = 1;
}

static void sig_handler_resize(int dummy __attribute__((__unused__)))
{
	sig_resize = 1;
}

static void menu_refresh_size(struct cfdisk *cf)
{
	if (cf->menu && cf->menu->nitems)
		cf->menu->page_sz = (cf->menu->nitems / (ui_lines - 4)) ? ui_lines - 4 : 0;
}

static void menu_update_ignore(struct cfdisk *cf)
{
	char ignore[128] = { 0 };
	int i = 0;
	struct cfdisk_menu *m;
	struct cfdisk_menuitem *d, *org = NULL;
	size_t idx;

	assert(cf);
	assert(cf->menu);
	assert(cf->menu->ignore_cb);

	m = cf->menu;
	DBG(MENU, ul_debug("update menu ignored keys"));

	i = m->ignore_cb(cf, ignore, sizeof(ignore));
	ignore[i] = '\0';

	/* return if no change */
	if ((!m->ignore && !*ignore)
	    || (m->ignore && *ignore && strcmp(m->ignore, ignore) == 0)) {
		    return;
	}

	if (!m->prefkey)
		org = menu_get_menuitem(cf, m->idx);

	free(m->ignore);
	m->ignore = xstrdup(ignore);
	m->nitems = 0;

	for (d = m->items; d->name; d++) {
		if (m->ignore && strchr(m->ignore, d->key))
			continue;
		m->nitems++;
	}

	DBG(MENU, ul_debug("update menu preferred keys"));

	/* refresh menu index to be at the same menuitem or go to the first */
	if (org && menu_get_menuitem_by_key(cf, org->key, &idx))
		m->idx = idx;
	else if (m->prefkey && menu_get_menuitem_by_key(cf, m->prefkey, &idx))
		m->idx = idx;
	else
		m->idx = 0;

	menu_refresh_size(cf);
}

static struct cfdisk_menu *menu_push(
			struct cfdisk *cf,
			struct cfdisk_menuitem *items)
{
	struct cfdisk_menu *m = xcalloc(1, sizeof(*m));
	struct cfdisk_menuitem *d;

	assert(cf);

	DBG(MENU, ul_debug("new menu"));

	m->prev = cf->menu;
	m->items = items;

	for (d = m->items; d->name; d++) {
		const char *name = _(d->name);
		size_t len = mbs_safe_width(name);
		if (len > m->width)
			m->width = len;
		m->nitems++;
	}

	cf->menu = m;

	menu_refresh_size(cf);
	return m;
}

static struct cfdisk_menu *menu_pop(struct cfdisk *cf)
{
	struct cfdisk_menu *m = NULL;

	assert(cf);

	DBG(MENU, ul_debug("pop menu"));

	if (cf->menu) {
		m = cf->menu->prev;
		free(cf->menu->ignore);
		free(cf->menu->title);
		free(cf->menu);
	}
	cf->menu = m;
	return cf->menu;
}

static void menu_set_title(struct cfdisk_menu *m, const char *title)
{
	char *str = NULL;

	if (title) {
		size_t len =  mbs_safe_width(title);
		if (len + MENU_TITLE_PADDING > m->width)
			m->width = len + MENU_TITLE_PADDING;
		str = xstrdup(title);
	}
	m->title = str;
}


static int ui_init(struct cfdisk *cf __attribute__((__unused__)))
{
	struct sigaction sa;

	DBG(UI, ul_debug("init"));

	/* setup SIGCHLD handler */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sig_handler_die;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	sa.sa_handler = sig_handler_resize;
	sigaction(SIGWINCH, &sa, NULL);

	ui_enabled = 1;
	initscr();

#ifdef HAVE_USE_DEFAULT_COLORS
	if (colors_wanted() && has_colors()) {
		size_t i;

		start_color();
		use_default_colors();
		for (i = 1; i < ARRAY_SIZE(color_pairs); i++)		/* yeah, start from 1! */
			init_pair(i, color_pairs[i][0], color_pairs[i][1]);
	}
#else
	colors_off();
#endif

	cbreak();
	noecho();
	nonl();
	curs_set(0);
	keypad(stdscr, TRUE);

	return 0;
}

/* "[ string ]" */
#define MENU_H_ITEMWIDTH(m)	(  MENU_H_PRESTR_SZ \
				 + MENU_H_SPADDING \
				 + (m)->width \
				 + MENU_H_SPADDING \
				 + MENU_H_POSTSTR_SZ)

#define MENU_V_ITEMWIDTH(m)	(MENU_V_SPADDING + (m)->width + MENU_V_SPADDING)


static size_t menuitem_get_line(struct cfdisk *cf, size_t idx)
{
	struct cfdisk_menu *m = cf->menu;

	if (m->vertical) {
		if (!m->page_sz)				/* small menu */
			return (ui_lines - (cf->menu->nitems + 1)) / 2 + idx;
		return (idx % m->page_sz) + 1;
	}

	{
		size_t len = MENU_H_ITEMWIDTH(m) + MENU_H_BETWEEN; /** item width */
		size_t items = ui_cols / len;			/* items per line */

		if (items == 0)
			return 0;
		return MENU_START_LINE + ((idx / items));
	}
}

static int menuitem_get_column(struct cfdisk *cf, size_t idx)
{
	if (cf->menu->vertical) {
		size_t nc = MENU_V_ITEMWIDTH(cf->menu);
		if ((size_t) ui_cols <= nc)
			return 0;
		return (ui_cols - nc) / 2;
	}

	{
		size_t len = MENU_H_ITEMWIDTH(cf->menu) + MENU_H_BETWEEN; /* item width */
		size_t items = ui_cols / len;				/* items per line */
		size_t extra = items < cf->menu->nitems ?		/* extra space on line */
				ui_cols % len :				/* - multi-line menu */
				ui_cols - (cf->menu->nitems * len);	/* - one line menu */

		if (items == 0)
			return 0;					/* hmm... no space */

		extra += MENU_H_BETWEEN;	/* add padding after last item to extra */

		if (idx < items)
			return (idx * len) + (extra / 2);
		return ((idx % items) * len) + (extra / 2);
	}
}

static int menuitem_on_page(struct cfdisk *cf, size_t idx)
{
	struct cfdisk_menu *m = cf->menu;

	if (m->page_sz == 0 ||
	    m->idx / m->page_sz == idx / m->page_sz)
		return 1;
	return 0;
}

static struct cfdisk_menuitem *menu_get_menuitem(struct cfdisk *cf, size_t idx)
{
	struct cfdisk_menuitem *d;
	size_t i;

	for (i = 0, d = cf->menu->items; d->name; d++) {
		if (cf->menu->ignore && strchr(cf->menu->ignore, d->key))
			continue;
		if (i++ == idx)
			return d;
	}

	return NULL;
}

static struct cfdisk_menuitem *menu_get_menuitem_by_key(struct cfdisk *cf,
							int key, size_t *idx)
{
	struct cfdisk_menuitem *d;

	for (*idx = 0, d = cf->menu->items; d->name; d++) {
		if (cf->menu->ignore && strchr(cf->menu->ignore, d->key))
			continue;
		if (key == d->key)
			return d;
		(*idx)++;
	}

	return NULL;
}

static void ui_draw_menuitem(struct cfdisk *cf,
			     struct cfdisk_menuitem *d,
			     size_t idx)
{
	char *buf, *ptr;
	const char *name;
	size_t width;
	const size_t buf_sz = 80 * MB_CUR_MAX;
	int ln, cl, vert = cf->menu->vertical;

	if (!menuitem_on_page(cf, idx))
		return;		/* no visible item */
	ln = menuitem_get_line(cf, idx);
	cl = menuitem_get_column(cf, idx);

	ptr = buf = xmalloc(buf_sz);
	/* string width */
	if (vert) {
		width = cf->menu->width + MENU_V_SPADDING;
		memset(ptr, ' ', MENU_V_SPADDING);
		ptr += MENU_V_SPADDING;
	} else
		width = MENU_H_SPADDING + cf->menu->width + MENU_H_SPADDING;

	name = _(d->name);
	mbsalign(name, ptr, buf_sz, &width,
			vert ? MBS_ALIGN_LEFT : MBS_ALIGN_CENTER,
			0);

	DBG(MENU, ul_debug("menuitem: cl=%d, ln=%d, item='%s'",
			cl, ln, buf));

	if (vert) {
		mvaddch(ln, cl - 1, ACS_VLINE);
		mvaddch(ln, cl + MENU_V_ITEMWIDTH(cf->menu), ACS_VLINE);
	}

	if (cf->menu->idx == idx)
		standout();

	if (vert)
		mvprintw(ln, cl, "%s", buf);
	else
		mvprintw(ln, cl, "%s%s%s", MENU_H_PRESTR, buf, MENU_H_POSTSTR);
	free(buf);

	if (cf->menu->idx == idx) {
		standend();
		if (d->desc)
			ui_hint("%s", _(d->desc));
	}
}

static void ui_clean_menu(struct cfdisk *cf)
{
	size_t i;
	size_t lastline;
	struct cfdisk_menu *m = cf->menu;
	size_t ln = menuitem_get_line(cf, 0);

	if (m->vertical)
		lastline = ln + (m->page_sz ? m->page_sz : m->nitems);
	else
		lastline = menuitem_get_line(cf, m->nitems);

	for (i = ln; i <= lastline; i++) {
		move(i, 0);
		clrtoeol();
		DBG(MENU, ul_debug("clean_menu: line %zu", i));
	}
	if (m->vertical) {
		move(ln - 1, 0);
		clrtoeol();
	}
	ui_clean_hint();
}

static void ui_draw_menu(struct cfdisk *cf)
{
	struct cfdisk_menuitem *d;
	struct cfdisk_menu *m;
	size_t i = 0;
	size_t ln = menuitem_get_line(cf, 0);
	size_t nlines;

	assert(cf);
	assert(cf->menu);

	DBG(MENU, ul_debug("draw start"));

	ui_clean_menu(cf);
	m = cf->menu;

	if (m->vertical)
		nlines = m->page_sz ? m->page_sz : m->nitems;
	else
		nlines = menuitem_get_line(cf, m->nitems);

	if (m->ignore_cb)
		menu_update_ignore(cf);
	i = 0;
	while ((d = menu_get_menuitem(cf, i)))
		ui_draw_menuitem(cf, d, i++);

	if (m->vertical) {
		size_t cl = menuitem_get_column(cf, 0);
		size_t curpg = m->page_sz ? m->idx / m->page_sz : 0;

		/* corners and horizontal lines */
		mvaddch(ln - 1, cl - 1, ACS_ULCORNER);
		mvaddch(ln + nlines, cl - 1, ACS_LLCORNER);

		for (i = 0; i < MENU_V_ITEMWIDTH(m); i++) {
			mvaddch(ln - 1, cl + i, ACS_HLINE);
			mvaddch(ln + nlines, cl + i, ACS_HLINE);
		}

		mvaddch(ln - 1, cl + i, ACS_URCORNER);
		mvaddch(ln + nlines, cl + i, ACS_LRCORNER);

		/* draw also lines around empty lines on last page */
		if (m->page_sz &&
		    m->nitems / m->page_sz == m->idx / m->page_sz) {
			for (i = m->nitems % m->page_sz + 1; i <= m->page_sz; i++) {
				mvaddch(i, cl - 1, ACS_VLINE);
				mvaddch(i, cl + MENU_V_ITEMWIDTH(m), ACS_VLINE);
			}
		}
		if (m->title) {
			attron(A_BOLD);
			mvprintw(ln - 1, cl, " %s ", m->title);
			attroff(A_BOLD);
		}
		if (curpg != 0)
			mvaddch(ln - 1, cl + MENU_V_ITEMWIDTH(m) - 2, ACS_UARROW);
		if (m->page_sz && curpg < m->nitems / m->page_sz)
			mvaddch(ln + nlines, cl + MENU_V_ITEMWIDTH(m) - 2, ACS_DARROW);
	}

	DBG(MENU, ul_debug("draw end."));
}

inline static int extra_insert_pair(struct cfdisk_line *l, const char *name, const char *data)
{
	struct libscols_line *lsl;
	int rc;

	assert(l);
	assert(l->extra);

	if (!data || !*data)
		return 0;

	lsl = scols_table_new_line(l->extra, NULL);
	if (!lsl)
		return -ENOMEM;

	rc = scols_line_set_data(lsl, 0, name);
	if (!rc)
		rc = scols_line_set_data(lsl, 1, data);

	return rc;
}

#ifndef HAVE_LIBMOUNT
static char *get_mountpoint(	struct cfdisk *cf __attribute__((unused)),
				const char *tagname __attribute__((unused)),
				const char *tagdata __attribute__((unused)))
{
	return NULL;
}
#else
static char *get_mountpoint(struct cfdisk *cf, const char *tagname, const char *tagdata)
{
	struct libmnt_fs *fs = NULL;
	char *target = NULL;
	int mounted = 0;

	assert(tagname);
	assert(tagdata);

	DBG(UI, ul_debug("asking for mountpoint [%s=%s]", tagname, tagdata));

	if (!cf->mntcache)
		cf->mntcache = mnt_new_cache();

	/* 1st try between mounted filesystems */
	if (!cf->mtab) {
		cf->mtab = mnt_new_table();
		if (cf->mtab) {
			mnt_table_set_cache(cf->mtab, cf->mntcache);
			mnt_table_parse_mtab(cf->mtab, NULL);
		}
	}

	if (cf->mtab)
		fs = mnt_table_find_tag(cf->mtab, tagname, tagdata, MNT_ITER_FORWARD);

	/* 2nd try fstab */
	if (!fs) {
		if (!cf->fstab) {
			cf->fstab = mnt_new_table();
			if (cf->fstab) {
				mnt_table_set_cache(cf->fstab, cf->mntcache);
				if (mnt_table_parse_fstab(cf->fstab, NULL) != 0) {
					mnt_unref_table(cf->fstab);
					cf->fstab = NULL;
				}
			}
		}
		if (cf->fstab)
			fs = mnt_table_find_tag(cf->fstab, tagname, tagdata, MNT_ITER_FORWARD);
	} else
		mounted = 1;

	if (fs) {
		if (mounted)
			xasprintf(&target, _("%s (mounted)"), mnt_fs_get_target(fs));
		else
			target = xstrdup(mnt_fs_get_target(fs));
	}

	return target;
}
#endif /* HAVE_LIBMOUNT */

static inline int iszero(const char *str)
{
	const char *p;

	for (p = str; p && *p == '0'; p++);

	return !p || *p == '\0';
}

static int has_uuid(struct fdisk_table *tb, const char *uuid)
{
	struct fdisk_partition *pa;
	struct fdisk_iter *itr;
	int rc = 0;

	if (!tb || !uuid || fdisk_table_is_empty(tb))
		return 0;

	itr = fdisk_new_iter(FDISK_ITER_FORWARD);
	while (rc == 0 && fdisk_table_next_partition(tb, itr, &pa) == 0) {
		const char *x = fdisk_partition_get_uuid(pa);
		if (x)
			rc = strcmp(x, uuid) == 0;
	}
	fdisk_free_iter(itr);
	return rc;
}

static void extra_prepare_data(struct cfdisk *cf)
{
	struct fdisk_partition *pa = get_current_partition(cf);
	struct cfdisk_line *l = &cf->lines[cf->lines_idx];
	char *data = NULL;
	char *mountpoint = NULL;

	DBG(UI, ul_debug("preparing extra data"));

	/* string data should not equal an empty string */
	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_NAME, &data) && data) {
		extra_insert_pair(l, _("Partition name:"), data);
		if (!mountpoint)
			mountpoint = get_mountpoint(cf, "PARTLABEL", data);
		free(data);
	}

	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_UUID, &data) && data) {
		extra_insert_pair(l, _("Partition UUID:"), data);

		/* Search for mountpoint by PARTUUID= means that we need to
		 * check fstab and convert PARTUUID to the device name. This is
		 * unnecessary and overkill for newly created partitions. Let's
		 * check if the UUID already exist in the old layout, otherwise
		 * ignore it.
		 */
		if (!mountpoint && has_uuid(cf->original_layout, data))
			mountpoint = get_mountpoint(cf, "PARTUUID", data);
		free(data);
	}

	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_TYPE, &data) && data) {
		char *code = NULL, *type = NULL;

		fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_TYPEID, &code);
		xasprintf(&type, "%s (%s)", data, code);

		extra_insert_pair(l, _("Partition type:"), type);
		free(data);
		free(code);
		free(type);
	}

	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_ATTR, &data) && data) {
		extra_insert_pair(l, _("Attributes:"), data);
		free(data);
	}

	/* for numeric data, only show non-zero rows */
	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_BSIZE, &data) && data) {
		if (!iszero(data))
			extra_insert_pair(l, "BSIZE:", data);
		free(data);
	}

	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_CPG, &data) && data) {
		if (!iszero(data))
			extra_insert_pair(l, "CPG:", data);
		free(data);
	}

	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_FSIZE, &data) && data) {
		if (!iszero(data))
			extra_insert_pair(l, "FSIZE:", data);
		free(data);
	}

	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_FSUUID, &data) && data) {
		extra_insert_pair(l, _("Filesystem UUID:"), data);
		if (!mountpoint)
			mountpoint = get_mountpoint(cf, "UUID", data);
		free(data);
	}

	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_FSLABEL, &data) && data) {
		extra_insert_pair(l, _("Filesystem LABEL:"), data);
		if (!mountpoint)
			mountpoint = get_mountpoint(cf, "LABEL", data);
		free(data);
	}
	if (!fdisk_partition_to_string(pa, cf->cxt, FDISK_FIELD_FSTYPE, &data) && data) {
		extra_insert_pair(l, _("Filesystem:"), data);
		free(data);
	}

	if (mountpoint) {
		extra_insert_pair(l, _("Mountpoint:"), mountpoint);
		free(mountpoint);
	}
}

static int ui_draw_extra(struct cfdisk *cf)
{
	WINDOW *win_ex;
	int wline = 1;
	struct cfdisk_line *ln = &cf->lines[cf->lines_idx];
	char *tbstr = NULL, *end;
	int win_ex_start_line, win_height, tblen;
	int ndatalines;

	if (!cf->show_extra)
		return 0;

	DBG(UI, ul_debug("draw extra"));

	assert(ln->extra);

	if (cf->act_win) {
		wclear(cf->act_win);
		touchwin(stdscr);
	}

	if (scols_table_is_empty(ln->extra)) {
		extra_prepare_data(cf);
		if (scols_table_is_empty(ln->extra))
			return 0;
	}

	ndatalines = fdisk_table_get_nents(cf->table) + 1;

	/* nents + header + one free line */
	win_ex_start_line = TABLE_START_LINE + ndatalines;
	win_height = MENU_START_LINE - win_ex_start_line;
	tblen = scols_table_get_nlines(ln->extra);

	/* we can't get a single line of data under the partlist*/
	if (win_height < 3)
		return 1;

	/* number of data lines + 2 for top/bottom lines */
	win_height = win_height < tblen + 2 ? win_height : tblen + 2;

	if ((size_t) win_ex_start_line + win_height + 1 < MENU_START_LINE)
		win_ex_start_line = MENU_START_LINE - win_height;

	win_ex = subwin(stdscr, win_height, ui_cols - 2, win_ex_start_line, 1);

	scols_table_reduce_termwidth(ln->extra, 4);
	scols_print_table_to_string(ln->extra, &tbstr);

	end = tbstr;
	while ((end = strchr(end, '\n')))
		*end++ = '\0';

	box(win_ex, 0, 0);

	end = tbstr;
	while (--win_height > 1) {
		mvwaddstr(win_ex, wline++, 1 /* window column*/, tbstr);
		tbstr += strlen(tbstr) + 1;
	}
	free(end);

	if (ln->w)
		delwin(ln->w);

	DBG(UI, ul_debug("draw window: %p", win_ex));
	touchwin(stdscr);
	wrefresh(win_ex);

	cf->act_win = ln->w = win_ex;
	return 0;
}

static void ui_menu_goto(struct cfdisk *cf, int where)
{
	struct cfdisk_menuitem *d;
	size_t old;

	/* stop and begin/end for vertical menus */
	if (cf->menu->vertical) {
		if (where < 0)
			where = 0;
		else if (where > (int) cf->menu->nitems - 1)
			where = cf->menu->nitems - 1;
	} else {
		/* continue from begin/end */
		if (where < 0)
			where = cf->menu->nitems - 1;
		else if ((size_t) where > cf->menu->nitems - 1)
			where = 0;
	}
	if ((size_t) where == cf->menu->idx)
		return;

	ui_clean_info();

	old = cf->menu->idx;
	cf->menu->idx = where;

	if (!menuitem_on_page(cf, old)) {
		ui_draw_menu(cf);
		return;
	}

	d = menu_get_menuitem(cf, old);
	ui_draw_menuitem(cf, d, old);

	d = menu_get_menuitem(cf, where);
	ui_draw_menuitem(cf, d, where);

}

static int ui_menu_move(struct cfdisk *cf, int key)
{
	struct cfdisk_menu *m;

	assert(cf);
	assert(cf->menu);

	if (key == (int) ERR)
		return 0;	/* ignore errors */

	m = cf->menu;

	DBG(MENU, ul_debug("menu move key >%c<.", key));

	if (m->vertical)
	{
		switch (key) {
		case KEY_DOWN:
		case '\016':	/* ^N */
		case 'j':	/* Vi-like alternative */
			ui_menu_goto(cf, m->idx + 1);
			return 0;
		case KEY_UP:
		case '\020':	/* ^P */
		case 'k':	/* Vi-like alternative */
			ui_menu_goto(cf, (int) m->idx - 1);
			return 0;
		case KEY_PPAGE:
			if (m->page_sz) {
				ui_menu_goto(cf, (int) m->idx - m->page_sz);
				return 0;
			}
			/* fallthrough */
		case KEY_HOME:
			ui_menu_goto(cf, 0);
			return 0;
		case KEY_NPAGE:
			if (m->page_sz) {
				ui_menu_goto(cf, m->idx + m->page_sz);
				return 0;
			}
			/* fallthrough */
		case KEY_END:
			ui_menu_goto(cf, m->nitems);
			return 0;
		}
	} else {
		switch (key) {
		case KEY_RIGHT:
		case '\t':
			ui_menu_goto(cf, m->idx + 1);
			return 0;
		case KEY_LEFT:
#ifdef KEY_BTAB
		case KEY_BTAB:
#endif
			ui_menu_goto(cf, (int) m->idx - 1);
			return 0;
		}
	}

	if (key == '\014') {		/* ^L refresh */
		ui_menu_resize(cf);
		return 0;
	}

	DBG(MENU, ul_debug(" no menu move key"));
	return 1;
}

/* but don't call me from ui_run(), this is for pop-up menus only */
static void ui_menu_resize(struct cfdisk *cf)
{
	DBG(MENU, ul_debug("menu resize/refresh"));
	resize();
	ui_clean_menu(cf);
	menu_refresh_size(cf);
	ui_draw_menu(cf);
	refresh();
}

static int partition_on_page(struct cfdisk *cf, size_t i)
{
	if (cf->page_sz == 0 ||
	    cf->lines_idx / cf->page_sz == i / cf->page_sz)
		return 1;
	return 0;
}

static void ui_draw_partition(struct cfdisk *cf, size_t i)
{
	int ln = TABLE_START_LINE + 1 + i;	/* skip table header */
	int cl = ARROW_CURSOR_WIDTH;		/* we need extra space for cursor */
	int cur = cf->lines_idx == i;
	size_t curpg = 0;

	if (cf->page_sz) {
		if (!partition_on_page(cf, i))
			return;
		ln = TABLE_START_LINE + (i % cf->page_sz) + 1;
		curpg = cf->lines_idx / cf->page_sz;
	}

	DBG(UI, ul_debug(
			"draw partition %zu [page_sz=%zu, "
			"line=%d, idx=%zu]",
			i, cf->page_sz, ln, cf->lines_idx));

	if (cur) {
		attron(A_REVERSE);
		mvaddstr(ln, 0, ARROW_CURSOR_STRING);
		mvaddstr(ln, cl, cf->lines[i + 1].data);
		attroff(A_REVERSE);
	} else {
		int at = 0;

		if (colors_wanted() && is_freespace(cf, i)) {
			attron(COLOR_PAIR(CFDISK_CL_FREESPACE));
			at = 1;
		}
		mvaddstr(ln, 0, ARROW_CURSOR_DUMMY);
		mvaddstr(ln, cl, cf->lines[i + 1].data);
		if (at)
			attroff(COLOR_PAIR(CFDISK_CL_FREESPACE));
	}

	if ((size_t) ln == MENU_START_LINE - 1 &&
	    cf->page_sz && curpg < cf->nlines / cf->page_sz) {
		if (cur)
			attron(A_REVERSE);
		mvaddch(ln, ui_cols - 1, ACS_DARROW);
		mvaddch(ln, 0, ACS_DARROW);
		if (cur)
			attroff(A_REVERSE);
	}

}

static int ui_draw_table(struct cfdisk *cf)
{
	int cl = ARROW_CURSOR_WIDTH;
	size_t i, nparts = fdisk_table_get_nents(cf->table);
	size_t curpg = cf->page_sz ? cf->lines_idx / cf->page_sz : 0;

	DBG(UI, ul_debug("draw table"));

	for (i = TABLE_START_LINE; i <= TABLE_START_LINE + cf->page_sz; i++) {
		move(i, 0);
		clrtoeol();
	}

	if (nparts == 0 || (size_t) cf->lines_idx > nparts - 1)
		cf->lines_idx = nparts ? nparts - 1 : 0;

	/* print header */
	attron(A_BOLD);
	mvaddstr(TABLE_START_LINE, cl, cf->lines[0].data);
	attroff(A_BOLD);

	/* print partitions */
	for (i = 0; i < nparts; i++)
		ui_draw_partition(cf, i);

	if (curpg != 0) {
		mvaddch(TABLE_START_LINE, ui_cols - 1, ACS_UARROW);
		mvaddch(TABLE_START_LINE, 0, ACS_UARROW);
	}
	if (cf->page_sz && curpg < cf->nlines / cf->page_sz) {
		mvaddch(MENU_START_LINE - 1, ui_cols - 1, ACS_DARROW);
		mvaddch(MENU_START_LINE - 1, 0, ACS_DARROW);
	}
	return 0;
}

static int ui_table_goto(struct cfdisk *cf, int where)
{
	size_t old;
	size_t nparts = fdisk_table_get_nents(cf->table);

	DBG(UI, ul_debug("goto table %d", where));

	if (where < 0)
		where = 0;
	if (!nparts)
		where = 0;
	else if ((size_t) where > nparts - 1)
		where = nparts - 1;

	if ((size_t) where == cf->lines_idx)
		return 0;

	old = cf->lines_idx;
	cf->lines_idx = where;

	if (!partition_on_page(cf, old) ||!partition_on_page(cf, where))
		ui_draw_table(cf);
	else {
		ui_draw_partition(cf, old);	/* cleanup old */
		ui_draw_partition(cf, where);	/* draw new */
	}
	ui_clean_info();
	ui_draw_menu(cf);
	ui_draw_extra(cf);
	refresh();

	return 0;
}

static int ui_refresh(struct cfdisk *cf)
{
	struct fdisk_label *lb;
	char *id = NULL;
        uint64_t bytes = fdisk_get_nsectors(cf->cxt) * fdisk_get_sector_size(cf->cxt);
	char *strsz;

	if (!ui_enabled)
		return -EINVAL;

	strsz = size_to_human_string(SIZE_DECIMAL_2DIGITS
				| SIZE_SUFFIX_SPACE
				| SIZE_SUFFIX_3LETTER, bytes);

	lb = fdisk_get_label(cf->cxt, NULL);
	assert(lb);

	clear();

	/* header */
	attron(A_BOLD);
	ui_center(0, _("Disk: %s"), fdisk_get_devname(cf->cxt));
	attroff(A_BOLD);
	ui_center(1, _("Size: %s, %"PRIu64" bytes, %ju sectors"),
			strsz, bytes, (uintmax_t) fdisk_get_nsectors(cf->cxt));
	if (fdisk_get_disklabel_id(cf->cxt, &id) == 0 && id)
		ui_center(2, _("Label: %s, identifier: %s"),
				fdisk_label_get_name(lb), id);
	else
		ui_center(2, _("Label: %s"), fdisk_label_get_name(lb));
	free(strsz);
	free(id);

	ui_draw_table(cf);
	ui_draw_menu(cf);
	refresh();
	return 0;
}

static ssize_t ui_get_string(const char *prompt,
			     const char *hint, char *buf, size_t len)
{
	int ln = MENU_START_LINE, cl = 1;
	ssize_t rc = -1;
	struct mbs_editor *edit;

	DBG(UI, ul_debug("ui get string"));

	assert(buf);
	assert(len);

	move(ln, 0);
	clrtoeol();

	move(ln + 1, 0);
	clrtoeol();

	if (prompt) {
		mvaddstr(ln, cl, prompt);
		cl += mbs_safe_width(prompt);
	}

	edit = mbs_new_edit(buf, len, ui_cols - cl);
	if (!edit)
		goto done;

	mbs_edit_goto(edit, MBS_EDIT_END);

	if (hint)
		ui_hint("%s", hint);
	else
		ui_clean_hint();

	curs_set(1);

	while (!sig_die) {
		wint_t c;	/* we have fallback in widechar.h */

		move(ln, cl);
		clrtoeol();
		mvaddstr(ln, cl, edit->buf);
		move(ln, cl + edit->cursor_cells);
		refresh();

#if !defined(HAVE_SLCURSES_H) && !defined(HAVE_SLANG_SLCURSES_H) && \
    defined(HAVE_LIBNCURSESW) && defined(HAVE_WIDECHAR)
		if (get_wch(&c) == ERR) {
#else
		if ((c = getch()) == (wint_t) ERR) {
#endif
			if (sig_die)
				break;
			if (sig_resize) {
				resize();
				continue;
			}
			if (!isatty(STDIN_FILENO))
				exit(2);
			else
				goto done;
		}

		DBG(UI, ul_debug("ui get string: key=%lc", c));

		if (c == '\r' || c == '\n' || c == KEY_ENTER)
			break;

		rc = 1;

		switch (c) {
		case KEY_ESC:
			rc = -CFDISK_ERR_ESC;
			goto done;
		case KEY_LEFT:
			rc = mbs_edit_goto(edit, MBS_EDIT_LEFT);
			break;
		case KEY_RIGHT:
			rc = mbs_edit_goto(edit, MBS_EDIT_RIGHT);
			break;
		case KEY_END:
			rc = mbs_edit_goto(edit, MBS_EDIT_END);
			break;
		case KEY_HOME:
			rc = mbs_edit_goto(edit, MBS_EDIT_HOME);
			break;
		case KEY_UP:
		case KEY_DOWN:
			break;
		case KEY_DC:
			rc = mbs_edit_delete(edit);
			break;
		case '\b':
		case KEY_DELETE:
		case KEY_BACKSPACE:
			rc = mbs_edit_backspace(edit);
			break;
		default:
			rc = mbs_edit_insert(edit, c);
			break;
		}
		if (rc == 1)
			beep();
	}

	if (sig_die)
		die_on_signal();

	rc = strlen(edit->buf);		/* success */
done:
	move(ln, 0);
	clrtoeol();
	curs_set(0);
	refresh();
	mbs_free_edit(edit);

	return rc;
}

static int ui_get_size(struct cfdisk *cf,	/* context */
		       const char *prompt,	/* UI dialog string */
		       uint64_t *res,		/* result in bytes */
		       uint64_t low,		/* minimal size */
		       uint64_t up,		/* maximal size */
		       int *expsize)		/* explicitly specified size */
{
	char buf[128];
	uint64_t user = 0;
	ssize_t rc;
	char *dflt = size_to_human_string(0, *res);

	DBG(UI, ul_debug("get_size (default=%"PRIu64")", *res));

	ui_clean_info();

	snprintf(buf, sizeof(buf), "%s", dflt);

	do {
		int pwr = 0, insec = 0;

		rc = ui_get_string(prompt,
				_("May be followed by M for MiB, G for GiB, "
				  "T for TiB, or S for sectors."),
				buf, sizeof(buf));
		ui_clean_warn();

		if (rc == 0) {
			ui_warnx(_("Please, specify size."));
			continue;			/* nothing specified */
		} if (rc == -CFDISK_ERR_ESC)
			break;				/* cancel dialog */

		if (strcmp(buf, dflt) == 0)
			user = *res, rc = 0;		/* no change, use default */
		else {
			size_t len = strlen(buf);
			if (buf[len - 1] == 'S' || buf[len - 1] == 's') {
				insec = 1;
				buf[len - 1] = '\0';
			}
			rc = parse_size(buf, (uintmax_t *)&user, &pwr);	/* parse */
		}

		if (rc == 0) {
			DBG(UI, ul_debug("get_size user=%"PRIu64", power=%d, in-sectors=%s",
						user, pwr, insec ? "yes" : "no"));
			if (insec)
				user *= fdisk_get_sector_size(cf->cxt);
			if (user < low) {
				ui_warnx(_("Minimum size is %"PRIu64" bytes."), low);
				rc = -ERANGE;
			}
			if (user > up && pwr && user < up + (1ULL << pwr * 10))
				/* ignore when the user specified size overflow
				 * with in range specified by suffix (e.g. MiB) */
				user = up;

			if (user > up) {
				ui_warnx(_("Maximum size is %"PRIu64" bytes."), up);
				rc = -ERANGE;
			}
			if (rc == 0 && insec && expsize)
				*expsize = 1;

		} else
			ui_warnx(_("Failed to parse size."));
	} while (rc != 0);

	if (rc == 0)
		*res = user;
	free(dflt);

	DBG(UI, ul_debug("get_size (result=%"PRIu64", rc=%zd)", *res, rc));
	return rc;
}

static struct fdisk_parttype *ui_get_parttype(struct cfdisk *cf,
					struct fdisk_parttype *cur)
{
	struct cfdisk_menuitem *d, *cm;
	size_t i = 0, nitems, idx = 0;
	struct fdisk_parttype *t = NULL;
	struct fdisk_label *lb;
	int codetypes = 0;

	DBG(UI, ul_debug("asking for parttype."));

	lb = fdisk_get_label(cf->cxt, NULL);

	/* create cfdisk menu according to label types, note that the
	 * last cm[] item has to be empty -- so nitems + 1 */
	nitems = fdisk_label_get_nparttypes(lb);
	if (!nitems)
		return NULL;

	cm = xcalloc(nitems + 1, sizeof(struct cfdisk_menuitem));
	if (!cm)
		return NULL;

	codetypes = fdisk_label_has_code_parttypes(lb);

	for (i = 0; i < nitems; i++) {
		const struct fdisk_parttype *x = fdisk_label_get_parttype(lb, i);
		char *name;

		cm[i].userdata = (void *) x;
		if (codetypes)
			xasprintf(&name, "%2x %s",
				fdisk_parttype_get_code(x),
				_(fdisk_parttype_get_name(x)));
		else {
			name = (char *) _(fdisk_parttype_get_name(x));
			cm[i].desc = fdisk_parttype_get_string(x);
		}
		cm[i].name = name;
		if (x == cur)
			idx = i;
	}

	/* make the new menu active */
	menu_push(cf, cm);
	cf->menu->vertical = 1;
	cf->menu->idx = idx;
	menu_set_title(cf->menu, _("Select partition type"));
	ui_draw_menu(cf);
	refresh();

	while (!sig_die) {
		int key = getch();

		if (sig_die)
			break;
		if (sig_resize)
			ui_menu_resize(cf);
		if (ui_menu_move(cf, key) == 0)
			continue;

		switch (key) {
		case KEY_ENTER:
		case '\n':
		case '\r':
			d = menu_get_menuitem(cf, cf->menu->idx);
			if (d)
				t = (struct fdisk_parttype *) d->userdata;
			goto done;
		case KEY_ESC:
		case 'q':
		case 'Q':
			goto done;
		}
	}

	if (sig_die)
		die_on_signal();
done:
	menu_pop(cf);
	if (codetypes) {
		for (i = 0; i < nitems; i++)
			free((char *) cm[i].name);
	}
	free(cm);
	DBG(UI, ul_debug("get parrtype done [type=%s] ", t ?
				fdisk_parttype_get_name(t) : ""));
	return t;
}

static int ui_script_read(struct cfdisk *cf)
{
	struct fdisk_script *sc = NULL;
	char buf[PATH_MAX] = { 0 };
	int rc;

	erase();
	rc = ui_get_string(	_("Enter script file name: "),
				_("The script file will be applied to in-memory partition table."),
				buf, sizeof(buf));
	if (rc <= 0)
		return rc;

	rc = -1;
	errno = 0;
	sc = fdisk_new_script_from_file(cf->cxt, buf);
	if (!sc && errno)
		ui_warn(_("Cannot open %s"), buf);
	else if (!sc)
		ui_warnx(_("Failed to parse script file %s"), buf);
	else if (fdisk_apply_script(cf->cxt, sc) != 0)
		ui_warnx(_("Failed to apply script %s"), buf);
	else
		rc = 0;

	ui_clean_hint();
	fdisk_unref_script(sc);
	return rc;
}

static int ui_script_write(struct cfdisk *cf)
{
	struct fdisk_script *sc = NULL;
	char buf[PATH_MAX] = { 0 };
	FILE *f = NULL;
	int rc;

	rc = ui_get_string(	_("Enter script file name: "),
				_("The current in-memory partition table will be dumped to the file."),
				buf, sizeof(buf));
	if (rc <= 0)
		return rc;

	rc = 0;
	sc = fdisk_new_script(cf->cxt);
	if (!sc) {
		ui_warn(_("Failed to allocate script handler"));
		goto done;
	}

	rc = fdisk_script_read_context(sc, NULL);
	if (rc) {
		ui_warnx(_("Failed to read disk layout into script."));
		goto done;
	}

	DBG(UI, ul_debug("writing dump into: '%s'", buf));
	f = fopen(buf, "w");
	if (!f) {
		ui_warn(_("Cannot open %s"), buf);
		rc = -errno;
		goto done;
	}

	rc = fdisk_script_write_file(sc, f);
	if (!rc)
		ui_info(_("Disk layout successfully dumped."));
done:
	if (rc)
		ui_warn(_("Failed to write script %s"), buf);
	if (f)
		fclose(f);
	fdisk_unref_script(sc);
	return rc;
}

/* prints menu with libfdisk labels and waits for users response */
static int ui_create_label(struct cfdisk *cf)
{
	struct cfdisk_menuitem *d, *cm;
	int rc = 1, refresh_menu = 1;
	size_t i = 0, nitems;
	struct fdisk_label *lb = NULL;

	assert(cf);

	DBG(UI, ul_debug("asking for new disklabe."));

	/* create cfdisk menu according to libfdisk labels, note that the
	 * last cm[] item has to be empty -- so nitems + 1 */
	nitems = fdisk_get_nlabels(cf->cxt);
	cm = xcalloc(nitems + 1, sizeof(struct cfdisk_menuitem));

	while (fdisk_next_label(cf->cxt, &lb) == 0) {
		if (fdisk_label_is_disabled(lb) ||
		    fdisk_label_get_type(lb) == FDISK_DISKLABEL_BSD)
			continue;
		cm[i++].name = fdisk_label_get_name(lb);
	}

	erase();

	/* make the new menu active */
	menu_push(cf, cm);
	cf->menu->vertical = 1;
	menu_set_title(cf->menu, _("Select label type"));

	if (!cf->zero_start)
		ui_info(_("Device does not contain a recognized partition table."));


	while (!sig_die) {
		int key;

		if (refresh_menu) {
			ui_draw_menu(cf);
			ui_hint(_("Select a type to create a new label, press 'L' to load script file, 'Q' quits."));
			refresh();
			refresh_menu = 0;
		}

		key = getch();

		if (sig_die)
			break;
		if (sig_resize)
			ui_menu_resize(cf);
		if (ui_menu_move(cf, key) == 0)
			continue;
		switch (key) {
		case KEY_ENTER:
		case '\n':
		case '\r':
			d = menu_get_menuitem(cf, cf->menu->idx);
			if (d)
				rc = fdisk_create_disklabel(cf->cxt, d->name);
			goto done;
		case KEY_ESC:
		case 'q':
		case 'Q':
			goto done;
		case 'l':
		case 'L':
			rc = ui_script_read(cf);
			if (rc == 0)
				goto done;
			refresh_menu = 1;
			break;
		}
	}

	if (sig_die)
		die_on_signal();
done:
	menu_pop(cf);
	free(cm);
	DBG(UI, ul_debug("create label done [rc=%d] ", rc));
	return rc;
}


static int ui_help(void)
{
	size_t i;
	static const char *help[] = {
		N_("This is cfdisk, a curses-based disk partitioning program."),
		N_("It lets you create, delete, and modify partitions on a block device."),
		"  ",
		N_("Command      Meaning"),
		N_("-------      -------"),
		N_("  b          Toggle bootable flag of the current partition;"),
		N_("               implemented for DOS (MBR) and SGI labels only"),
		N_("  d          Delete the current partition"),
		N_("  h          Print this screen"),
		N_("  n          Create new partition from free space"),
		N_("  q          Quit program without writing partition table"),
		N_("  r          Reduce or enlarge the current partition"),
		N_("  s          Fix partitions order (only when in disarray)"),
		N_("  t          Change the partition type"),
		N_("  u          Dump disk layout to sfdisk compatible script file"),
		N_("  W          Write partition table to disk (you must enter uppercase W);"),
		N_("               since this might destroy data on the disk, you must either"),
		N_("               confirm or deny the write by entering 'yes' or 'no'"),
		N_("  x          Display/hide extra information about a partition"),
		N_("Up Arrow     Move cursor to the previous partition"),
		N_("Down Arrow   Move cursor to the next partition"),
		N_("Left Arrow   Move cursor to the previous menu item"),
		N_("Right Arrow  Move cursor to the next menu item"),
		"  ",
		N_("Note: All of the commands can be entered with either upper or lower"),
		N_("case letters (except for Write)."),
		"  ",
		N_("Use lsblk(8) or partx(8) to see more details about the device."),
		"  ",
		"  ",
		"Copyright (C) 2014-2023 Karel Zak <kzak@redhat.com>"
	};

	erase();
	for (i = 0; i < ARRAY_SIZE(help); i++)
		mvaddstr(i, 1, _(help[i]));

	ui_info(_("Press a key to continue."));

	getch();

	if (sig_die)
		die_on_signal();
	return 0;
}

/* TODO: use @sz, now 128bytes */
static int main_menu_ignore_keys(struct cfdisk *cf, char *ignore,
		size_t sz __attribute__((__unused__)))
{
	struct fdisk_partition *pa = get_current_partition(cf);
	size_t i = 0;

	if (!pa)
		return 0;
	if (fdisk_partition_is_freespace(pa)) {
		ignore[i++] = 'd';	/* delete */
		ignore[i++] = 't';	/* set type */
		ignore[i++] = 'b';      /* set bootable */
		ignore[i++] = 'r';	/* resize */
		cf->menu->prefkey = 'n';
	} else {
		cf->menu->prefkey = 'q';
		ignore[i++] = 'n';
		if (!fdisk_is_label(cf->cxt, DOS) &&
		    !fdisk_is_label(cf->cxt, SGI))
			ignore[i++] = 'b';
	}

	if (!cf->wrong_order)
		ignore[i++] = 's';

	if (fdisk_is_readonly(cf->cxt))
		ignore[i++] = 'W';

	return i;
}


/* returns: error: < 0, success: 0, quit: 1 */
static int main_menu_action(struct cfdisk *cf, int key)
{
	size_t n;
	int ref = 0, rc, org_order = cf->wrong_order;
	const char *info = NULL, *warn = NULL;
	struct fdisk_partition *pa;

	assert(cf);
	assert(cf->cxt);
	assert(cf->menu);

	if (key == 0) {
		struct cfdisk_menuitem *d = menu_get_menuitem(cf, cf->menu->idx);
		if (!d)
			return 0;
		key = d->key;

	} else if (key != 'w' && key != 'W')
		key = tolower(key);	/* case insensitive except 'W'rite */

	DBG(MENU, ul_debug("main menu action: key=%c", key));

	if (cf->menu->ignore && strchr(cf->menu->ignore, key)) {
		DBG(MENU, ul_debug("  ignore '%c'", key));
		return 0;
	}

	pa = get_current_partition(cf);
	if (!pa)
		return -EINVAL;
	n = fdisk_partition_get_partno(pa);

	DBG(MENU, ul_debug("menu action on %p", pa));
	ui_clean_hint();
	ui_clean_info();

	switch (key) {
	case 'b': /* Bootable flag */
	{
		int fl = fdisk_is_label(cf->cxt, DOS) ? DOS_FLAG_ACTIVE :
			 fdisk_is_label(cf->cxt, SGI) ? SGI_FLAG_BOOT : 0;

		if (fl && fdisk_toggle_partition_flag(cf->cxt, n, fl))
			warn = _("Could not toggle the flag.");
		else if (fl)
			ref = 1;
		break;
	}
#ifdef KEY_DC
	case KEY_DC:
#endif
	case 'd': /* Delete */
		if (fdisk_delete_partition(cf->cxt, n) != 0)
			warn = _("Could not delete partition %zu.");
		else
			info = _("Partition %zu has been deleted.");
		ref = 1;
		break;
	case 'h': /* Help */
	case '?':
		ui_help();
		ref = 1;
		break;
	case 'n': /* New */
	{
		uint64_t start, size, dflt_size, secs, max_size;
		struct fdisk_partition *npa;	/* the new partition */
		int expsize = 0;		/* size specified explicitly in sectors */

		if (!fdisk_partition_is_freespace(pa) || !fdisk_partition_has_start(pa))
			return -EINVAL;

		/* free space range */
		start = fdisk_partition_get_start(pa);
		size = max_size = dflt_size = fdisk_partition_get_size(pa) * fdisk_get_sector_size(cf->cxt);

		if (ui_get_size(cf, _("Partition size: "), &size,
				fdisk_get_sector_size(cf->cxt),
				max_size, &expsize) == -CFDISK_ERR_ESC)
			break;

		secs = size / fdisk_get_sector_size(cf->cxt);

		npa = fdisk_new_partition();
		if (!npa)
			return -ENOMEM;

		if (dflt_size == size)	/* default is to fillin all free space */
			fdisk_partition_end_follow_default(npa, 1);
		else
			fdisk_partition_set_size(npa, secs);

		if (expsize)
			fdisk_partition_size_explicit(pa, 1);

		fdisk_partition_set_start(npa, start);
		fdisk_partition_partno_follow_default(npa, 1);
		/* add to disk label -- libfdisk will ask for missing details */
		rc = fdisk_add_partition(cf->cxt, npa, NULL);
		fdisk_unref_partition(npa);
		if (rc == 0)
			ref = 1;
		break;
	}
	case 'q': /* Quit */
		return 1;
	case 't': /* Type */
	{
		struct fdisk_parttype *t;

		if (fdisk_partition_is_freespace(pa))
			return -EINVAL;
		t = (struct fdisk_parttype *) fdisk_partition_get_type(pa);
		t = ui_get_parttype(cf, t);
		ref = 1;

		if (t && fdisk_set_partition_type(cf->cxt, n, t) == 0)
			info = _("Changed type of partition %zu.");
		else
			info = _("The type of partition %zu is unchanged.");
		break;
	}
	case 'r': /* resize */
	{
		uint64_t size, max_size, secs;
		struct fdisk_partition *npa;

		if (fdisk_partition_is_freespace(pa) || !fdisk_partition_has_start(pa))
			return -EINVAL;

		rc = fdisk_partition_get_max_size(cf->cxt,
						  fdisk_partition_get_partno(pa),
						  &size);
		if (rc)
			return rc;

		size *= fdisk_get_sector_size(cf->cxt);
		max_size = size;

		if (ui_get_size(cf, _("New size: "), &size,
				fdisk_get_sector_size(cf->cxt),
				max_size, NULL) == -CFDISK_ERR_ESC)
			break;
		secs = size / fdisk_get_sector_size(cf->cxt);
		npa = fdisk_new_partition();
		if (!npa)
			return -ENOMEM;

		fdisk_partition_set_size(npa, secs);

		rc = fdisk_set_partition(cf->cxt, n, npa);
		fdisk_unref_partition(npa);
		if (rc == 0) {
			ref = 1;
			info = _("Partition %zu resized.");
		}
		break;
	}
	case 's': /* Sort */
		if (cf->wrong_order) {
			fdisk_reorder_partitions(cf->cxt);
			ref = 1;
		}
		break;
	case 'u': /* dUmp */
		ui_script_write(cf);
		break;
	case 'W': /* Write */
	{
		char buf[64] = { 0 };

		if (fdisk_is_readonly(cf->cxt)) {
			warn = _("Device is open in read-only mode.");
			break;
		}

		rc = ui_get_string(
			  _("Are you sure you want to write the partition "
			    "table to disk? "),
			  _("Type \"yes\" or \"no\", or press ESC to leave this dialog."),
			  buf, sizeof(buf));

		ref = 1;
		if (rc <= 0 || (strcasecmp(buf, "yes") != 0 &&
				strcasecmp(buf, _("yes")) != 0)) {
			info = _("Did not write partition table to disk.");
			break;
		}
		rc = fdisk_write_disklabel(cf->cxt);
		if (rc)
			warn = _("Failed to write disklabel.");
		else {
			size_t q_idx = 0;

			if (cf->device_is_used)
				fdisk_reread_changes(cf->cxt, cf->original_layout);
			else
				fdisk_reread_partition_table(cf->cxt);
			info = _("The partition table has been altered.");
			if (menu_get_menuitem_by_key(cf, 'q', &q_idx))
				ui_menu_goto(cf, q_idx);
		}
		cf->nwrites++;
		break;
	}
	default:
		break;
	}

	if (ref) {
		lines_refresh(cf);
		ui_refresh(cf);
		ui_draw_extra(cf);
	} else
		ui_draw_menu(cf);

	ui_clean_hint();

	if (warn)
		ui_warnx(warn, n + 1);
	else if (info)
		ui_info(info, n + 1);
	else if (key == 'n' && cf->wrong_order && org_order == 0)
		 ui_info(_("Note that partition table entries are not in disk order now."));

	return 0;
}

static void ui_resize_refresh(struct cfdisk *cf)
{
	DBG(UI, ul_debug("ui resize/refresh"));
	resize();
	menu_refresh_size(cf);
	lines_refresh(cf);
	ui_refresh(cf);
	ui_draw_extra(cf);
}

static void toggle_show_extra(struct cfdisk *cf)
{
	if (cf->show_extra && cf->act_win) {
		wclear(cf->act_win);
		touchwin(stdscr);
	}
	cf->show_extra = cf->show_extra ? 0 : 1;

	if (cf->show_extra)
		ui_draw_extra(cf);
	DBG(MENU, ul_debug("extra: %s", cf->show_extra ? "ENABLED" : "DISABLED" ));
}

static int ui_run(struct cfdisk *cf)
{
	int rc = 0;

	ui_lines = LINES;
	ui_cols = COLS;
	DBG(UI, ul_debug("start cols=%zu, lines=%zu", ui_cols, ui_lines));

	if (fdisk_get_collision(cf->cxt)) {
		ui_warnx(_("Device already contains a %s signature."), fdisk_get_collision(cf->cxt));
		if (fdisk_is_readonly(cf->cxt)) {
			ui_hint(_("Press a key to continue."));
			getch();
		} else {
			char buf[64] = { 0 };
			rc = ui_get_string(_("Do you want to remove it? [Y]es/[N]o: "), NULL,
					buf, sizeof(buf));
			fdisk_enable_wipe(cf->cxt,
					rc > 0 && rpmatch(buf) == RPMATCH_YES ? 1 : 0);
		}
	}

	if (!fdisk_has_label(cf->cxt) || cf->zero_start) {
		rc = ui_create_label(cf);
		if (rc < 0) {
			errno = -rc;
			ui_err(EXIT_FAILURE,
					_("failed to create a new disklabel"));
		}
		if (rc)
			return rc;
	}

	cols_init(cf);
	rc = lines_refresh(cf);
	if (rc)
		ui_errx(EXIT_FAILURE, _("failed to read partitions"));

	menu_push(cf, main_menuitems);
	cf->menu->ignore_cb = main_menu_ignore_keys;

	rc = ui_refresh(cf);
	if (rc)
		return rc;

	cf->show_extra = 1;
	ui_draw_extra(cf);

	if (fdisk_is_readonly(cf->cxt))
		ui_warnx(_("Device is open in read-only mode. Changes will remain in memory only."));
	else if (cf->device_is_used)
		ui_warnx(_("Device is currently in use, repartitioning is probably a bad idea."));
	else if (cf->wrong_order)
		ui_info(_("Note that partition table entries are not in disk order now."));

	while (!sig_die) {
		int key = getch();

		rc = 0;

		if (sig_die)
			break;
		if (sig_resize)
			/* Note that ncurses getch() returns ERR when interrupted
			 * by signal, but SLang does not interrupt at all. */
			ui_resize_refresh(cf);
		if (key == ERR)
			continue;
		if (key == '\014') {		/* ^L refresh */
			ui_resize_refresh(cf);
			continue;
		}
		if (ui_menu_move(cf, key) == 0)
			continue;

		DBG(UI, ul_debug("main action key >%1$c< [\\0%1$o].", key));

		switch (key) {
		case KEY_DOWN:
		case '\016':	/* ^N */
		case 'j':	/* Vi-like alternative */
			ui_table_goto(cf, cf->lines_idx + 1);
			break;
		case KEY_UP:
		case '\020':	/* ^P */
		case 'k':	/* Vi-like alternative */
			ui_table_goto(cf, (int) cf->lines_idx - 1);
			break;
		case KEY_PPAGE:
			if (cf->page_sz) {
				ui_table_goto(cf, (int) cf->lines_idx - cf->page_sz);
				break;
			}
			/* fallthrough */
		case KEY_HOME:
			ui_table_goto(cf, 0);
			break;
		case KEY_NPAGE:
			if (cf->page_sz) {
				ui_table_goto(cf, cf->lines_idx + cf->page_sz);
				break;
			}
			/* fallthrough */
		case KEY_END:
			ui_table_goto(cf, (int) cf->nlines - 1);
			break;
		case KEY_ENTER:
		case '\n':
		case '\r':
			rc = main_menu_action(cf, 0);
			break;
		case 'X':
		case 'x': /* Extra */
			toggle_show_extra(cf);
			break;
		default:
			rc = main_menu_action(cf, key);
			if (rc < 0)
				beep();
			break;
		}

		if (rc == 1)
			break; /* quit */
	}

	menu_pop(cf);

	DBG(UI, ul_debug("end"));
	return 0;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %1$s [options] <disk>\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Display or manipulate a disk partition table.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out,
	      _(" -L, --color[=<when>]     colorize output (%s, %s or %s)\n"), "auto", "always", "never");
	fprintf(out,
	        "                            %s\n", USAGE_COLORS_DEFAULT);
	fputs(_(" -z, --zero               start with zeroed partition table\n"), out);
	fprintf(out,
	      _("     --lock[=<mode>]      use exclusive device lock (%s, %s or %s)\n"), "yes", "no", "nonblock");
	fputs(_(" -r, --read-only          forced open cfdisk in read-only mode\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(26));

	fprintf(out, USAGE_MAN_TAIL("cfdisk(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	const char *diskpath = NULL, *lockmode = NULL;
	int rc, c, colormode = UL_COLORMODE_UNDEF;
	int read_only = 0;
	struct cfdisk _cf = { .lines_idx = 0 },
		      *cf = &_cf;
	enum {
		OPT_LOCK	= CHAR_MAX + 1
	};
	static const struct option longopts[] = {
		{ "color",   optional_argument, NULL, 'L' },
		{ "lock",    optional_argument, NULL, OPT_LOCK },
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'V' },
		{ "zero",    no_argument,	NULL, 'z' },
		{ "read-only", no_argument,     NULL, 'r' },
		{ NULL, 0, NULL, 0 },
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while((c = getopt_long(argc, argv, "L::hVzr", longopts, NULL)) != -1) {
		switch(c) {
		case 'h':
			usage();
			break;
		case 'L':
			colormode = UL_COLORMODE_AUTO;
			if (optarg)
				colormode = colormode_or_err(optarg,
						_("unsupported color mode"));
			break;
                case 'r':
                        read_only = 1;
                        break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'z':
			cf->zero_start = 1;
			break;
		case OPT_LOCK:
			lockmode = "1";
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				lockmode = optarg;
			}
			break;
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	colors_init(colormode, "cfdisk");

	fdisk_init_debug(0);
	scols_init_debug(0);
	cfdisk_init_debug();
	cf->cxt = fdisk_new_context();
	if (!cf->cxt)
		err(EXIT_FAILURE, _("failed to allocate libfdisk context"));

	fdisk_set_ask(cf->cxt, ask_callback, (void *) cf);

	if (optind == argc) {
		size_t i;

		for (i = 0; i < ARRAY_SIZE(default_disks); i++) {
			if (access(default_disks[i], F_OK) == 0) {
				diskpath = default_disks[i];
				break;
			}
		}
		if (!diskpath)
			diskpath = default_disks[0];	/* default, used for "cannot open" */
	} else
		diskpath = argv[optind];

	rc = fdisk_assign_device(cf->cxt, diskpath, read_only);
	if (rc == -EACCES && read_only == 0)
		rc = fdisk_assign_device(cf->cxt, diskpath, 1);
	if (rc != 0)
		err(EXIT_FAILURE, _("cannot open %s"), diskpath);

	if (!fdisk_is_readonly(cf->cxt)) {
		if (blkdev_lock(fdisk_get_devfd(cf->cxt), diskpath, lockmode) != 0)
			return EXIT_FAILURE;

		cf->device_is_used = fdisk_device_is_used(cf->cxt);
		fdisk_get_partitions(cf->cxt, &cf->original_layout);
	}

	/* Don't use err(), warn() from this point */
	ui_init(cf);
	ui_run(cf);
	ui_end();

	cfdisk_free_lines(cf);
	free(cf->linesbuf);
	free(cf->fields);

	fdisk_unref_table(cf->table);
#ifdef HAVE_LIBMOUNT
	mnt_unref_table(cf->fstab);
	mnt_unref_table(cf->mtab);
	mnt_unref_cache(cf->mntcache);
#endif
	rc = fdisk_deassign_device(cf->cxt, cf->nwrites == 0);
	fdisk_unref_context(cf->cxt);
	DBG(MISC, ul_debug("bye! [rc=%d]", rc));
	return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
