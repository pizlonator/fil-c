/*
 * slabtop.c - utility to display kernel slab information.
 *
 * Copyright © 2009-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 2002-2004 Albert Cahalan
 * Copyright © 2003      Chris Rivera <cmrivera@ufl.edu>
 * Copyright © 2003      Robert Love <rml@tech9.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "strutils.h"

#include "slabinfo.h"

#define DEFAULT_SORT  SLAB_NUM_OBJS
#define CHAINS_ALLOC  150
#define MAXTBL(t) (int)( sizeof(t) / sizeof(t[0]) )
#define DEFAULT_DELAY 3

static unsigned short Cols, Rows;
static struct termios Saved_tty;
static long Delay = 0;
static int Run_once = 0;

static struct slabinfo_info *Slab_info;

enum slabinfo_item Sort_item = DEFAULT_SORT;
enum slabinfo_sort_order Sort_Order = SLABINFO_SORT_DESCEND;

enum slabinfo_item Node_items[] = {
    SLAB_NUM_OBJS,   SLAB_ACTIVE_OBJS, SLAB_PERCENT_USED,
    SLAB_OBJ_SIZE,   SLAB_NUMS_SLABS,  SLAB_OBJ_PER_SLAB,
    SLAB_SIZE_TOTAL, SLAB_NAME,
    /* next 2 are sortable but are not displayable,
       thus they need not be represented in the Relative_enums */
    SLAB_PAGES_PER_SLAB, SLAB_ACTIVE_SLABS };

enum Relative_node {
    nod_OBJS,  nod_AOBJS, nod_USE,  nod_OSIZE,
    nod_SLABS, nod_OPS,   nod_SIZE, nod_NAME };

#define MAX_ITEMS (int)(sizeof(Node_items) / sizeof(Node_items[0]))

#define PRINT_line(fmt, ...) if (Run_once) printf(fmt, __VA_ARGS__); else printw(fmt, __VA_ARGS__)


/*
 * term_resize - set the globals 'Cols' and 'Rows' to the current terminal size
 */
static void term_resize (int unusused __attribute__ ((__unused__)))
{
    struct winsize ws;

    if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) && ws.ws_row > 10) {
        Cols = ws.ws_col;
        Rows = ws.ws_row;
    } else {
        Cols = 80;
        Rows = 24;
    }
}

static void sigint_handler (int unused __attribute__ ((__unused__)))
{
    Delay = 0;
}

static void __attribute__((__noreturn__)) usage (FILE *out)
{
    fputs(USAGE_HEADER, out);
    fprintf(out, _(" %s [options]\n"), program_invocation_short_name);
    fputs(USAGE_OPTIONS, out);
    fputs(_(" -d, --delay <secs>  delay updates\n"), out);
    fputs(_(" -o, --once          only display once, then exit\n"), out);
    fputs(_(" -s, --sort <char>   specify sort criteria by character (see below)\n"), out);
    fputs(USAGE_SEPARATOR, out);
    fputs(USAGE_HELP, out);
    fputs(USAGE_VERSION, out);

    fputs(_("\nThe following are valid sort criteria:\n"), out);
    fputs(_(" a: sort by number of active objects\n"), out);
    fputs(_(" b: sort by objects per slab\n"), out);
    fputs(_(" c: sort by cache size\n"), out);
    fputs(_(" l: sort by number of slabs\n"), out);
    fputs(_(" v: sort by (non display) number of active slabs\n"), out);
    fputs(_(" n: sort by name\n"), out);
    fputs(_(" o: sort by number of objects (the default)\n"), out);
    fputs(_(" p: sort by (non display) pages per slab\n"), out);
    fputs(_(" s: sort by object size\n"), out);
    fputs(_(" u: sort by cache utilization\n"), out);
    fprintf(out, USAGE_MAN_TAIL("slabtop(1)"));

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void set_sort_stuff (const char key)
{
    Sort_item = DEFAULT_SORT;
    Sort_Order = SLABINFO_SORT_DESCEND;

    switch (tolower(key)) {
    case 'n':
        Sort_item = SLAB_NAME;
        Sort_Order = SLABINFO_SORT_ASCEND;
        break;
    case 'o':
        Sort_item = SLAB_NUM_OBJS;
        break;
    case 'a':
        Sort_item = SLAB_ACTIVE_OBJS;
        break;
    case 's':
        Sort_item = SLAB_OBJ_SIZE;
        break;
    case 'b':
        Sort_item = SLAB_OBJ_PER_SLAB;
        break;
    case 'p':
        Sort_item = SLAB_PAGES_PER_SLAB;
        break;
    case 'l':
        Sort_item = SLAB_NUMS_SLABS;
        break;
    case 'v':
        Sort_item = SLAB_ACTIVE_SLABS;
        break;
    case 'c':
        Sort_item = SLAB_SIZE_TOTAL;
        break;
    case 'u':
        Sort_item = SLAB_PERCENT_USED;
        break;
    default:
        break;
    }
}

static void parse_opts (int argc, char **argv)
{
    static const struct option longopts[] = {
        { "delay",   required_argument, NULL, 'd' },
        { "sort",    required_argument, NULL, 's' },
        { "once",    no_argument,       NULL, 'o' },
        { "help",    no_argument,       NULL, 'h' },
        { "version", no_argument,       NULL, 'V' },
        {  NULL,     0,                 NULL,  0  }};
    int o;

    while ((o = getopt_long(argc, argv, "d:s:ohV", longopts, NULL)) != -1) {
        switch (o) {
        case 'd':
	    if (Run_once)
		xerrx(EXIT_FAILURE, _("Cannot combine -d and -o options"));
            errno = 0;
            Delay = strtol_or_err(optarg, _("illegal delay"));
            if (Delay < 1)
                xerrx(EXIT_FAILURE, _("delay must be positive integer"));
            break;
        case 's':
            set_sort_stuff(optarg[0]);
            break;
        case 'o':
	    if (Delay != 0)
		xerrx(EXIT_FAILURE, _("Cannot combine -d and -o options"));
            Run_once=1;
            break;
        case 'V':
            printf(PROCPS_NG_VERSION);
            exit(EXIT_SUCCESS);
        case 'h':
            usage(stdout);
        default:
            usage(stderr);
        }
    }
    if (optind != argc)
        usage(stderr);
    if (!Run_once && Delay == 0)
	Delay = DEFAULT_DELAY;
}

static void print_summary (void)
{
 #define totalVAL(e,t) SLABINFO_VAL(e, t, p, Slab_info)
    enum slabinfo_item items[] = {
        SLABS_ACTIVE_OBJS,   SLABS_NUM_OBJS,
        SLABS_ACTIVE_SLABS,  SLABS_NUMS_SLABS,
        SLABS_CACHES_ACTIVE, SLABS_CACHES_TOTAL,
        SLABS_SIZE_ACTIVE,   SLABS_SIZE_TOTAL,
        SLABS_OBJ_SIZE_MIN,  SLABS_OBJ_SIZE_AVG,
        SLABS_OBJ_SIZE_MAX
    };
    enum rel_items {
        tot_AOBJS,   tot_OBJS,   tot_ASLABS, tot_SLABS,
        tot_ACACHES, tot_CACHES, tot_ACTIVE, tot_TOTAL,
        tot_MIN,     tot_AVG,    tot_MAX
    };
    struct slabinfo_stack *p;

    if (!(p = procps_slabinfo_select(Slab_info, items, MAXTBL(items))))
        xerrx(EXIT_FAILURE, _("Error getting slab summary results"));

    PRINT_line(" %-35s: %u / %u (%.1f%%)\n"
               , /* Translation Hint: Next five strings must not
                  * exceed a length of 35 characters.  */
                 /* xgettext:no-c-format */
                 _("Active / Total Objects (% used)")
               , totalVAL(tot_AOBJS, u_int)
               , totalVAL(tot_OBJS,  u_int)
               , 100.0 * totalVAL(tot_AOBJS, u_int) / totalVAL(tot_OBJS, u_int));
    PRINT_line(" %-35s: %u / %u (%.1f%%)\n"
               , /* xgettext:no-c-format */
                 _("Active / Total Slabs (% used)")
               , totalVAL(tot_ASLABS, u_int)
               , totalVAL(tot_SLABS,  u_int)
               , 100.0 * totalVAL(tot_ASLABS, u_int) / totalVAL(tot_SLABS, u_int));
    PRINT_line(" %-35s: %u / %u (%.1f%%)\n"
               , /* xgettext:no-c-format */
                 _("Active / Total Caches (% used)")
               , totalVAL(tot_ACACHES, u_int)
               , totalVAL(tot_CACHES,  u_int)
               , 100.0 * totalVAL(tot_ACACHES, u_int) / totalVAL(tot_CACHES, u_int));
    PRINT_line(" %-35s: %.2fK / %.2fK (%.1f%%)\n"
               , /* xgettext:no-c-format */
                 _("Active / Total Size (% used)")
               , totalVAL(tot_ACTIVE, ul_int) / 1024.0
               , totalVAL(tot_TOTAL,  ul_int) / 1024.0
               , 100.0 * totalVAL(tot_ACTIVE, ul_int) / totalVAL(tot_TOTAL, ul_int));
    PRINT_line(" %-35s: %.2fK / %.2fK / %.2fK\n\n"
               , _("Minimum / Average / Maximum Object")
               , totalVAL(tot_MIN, u_int) / 1024.0
               , totalVAL(tot_AVG, u_int) / 1024.0
               , totalVAL(tot_MAX, u_int) / 1024.0);
 #undef totalVAL
}

static void print_headings (void)
{
    /* Translation Hint: Please keep alignment of the
     * following intact. */
    PRINT_line("%-78s\n", _("  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME"));
}

static void print_details (struct slabinfo_stack *stack)
{
 #define nodeVAL(e,t) SLABINFO_VAL(e, t, stack, Slab_info)
    PRINT_line("%6u %6u %3u%% %7.2fK %6u %8u %9luK %-23s\n"
        , nodeVAL(nod_OBJS,  u_int)
        , nodeVAL(nod_AOBJS, u_int)
        , nodeVAL(nod_USE,   u_int)
        , nodeVAL(nod_OSIZE, u_int) / 1024.0
        , nodeVAL(nod_SLABS, u_int)
        , nodeVAL(nod_OPS,   u_int)
        , nodeVAL(nod_SIZE,  ul_int) / 1024
        , nodeVAL(nod_NAME,  str));

    return;
 #undef nodeVAL
}


int main(int argc, char *argv[])
{
    int is_tty = 0, rc = EXIT_SUCCESS;
    unsigned short old_rows = 0;

#ifdef HAVE_PROGRAM_INVOCATION_NAME
    program_invocation_name = program_invocation_short_name;
#endif
    setlocale (LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    atexit(close_stdout);

    parse_opts(argc, argv);

    if (procps_slabinfo_new(&Slab_info) < 0)
        xerr(EXIT_FAILURE, _("Unable to create slabinfo structure"));

    if (!Run_once) {
        is_tty = isatty(STDIN_FILENO);
        if (is_tty && tcgetattr(STDIN_FILENO, &Saved_tty) == -1)
            xwarn(_("terminal setting retrieval"));
        old_rows = Rows;
        term_resize(0);
        initscr();
        resizeterm(Rows, Cols);
        signal(SIGWINCH, term_resize);
        signal(SIGINT, sigint_handler);
    }

    do {
        struct slabinfo_reaped *reaped;
        struct timeval tv;
        fd_set readfds;
        int i;

        if (!(reaped = procps_slabinfo_reap(Slab_info, Node_items, MAXTBL(Node_items)))) {
            xwarn(_("Unable to get slabinfo node data"));
            rc = EXIT_FAILURE;
            break;
        }

        if (!(procps_slabinfo_sort(Slab_info, reaped->stacks, reaped->total, Sort_item, Sort_Order))) {
            xwarn(_("Unable to sort slab nodes"));
            rc = EXIT_FAILURE;
            break;
        }

        if (Run_once) {
            print_summary();
            print_headings();
            for (i = 0; i < reaped->total; i++)
                print_details(reaped->stacks[i]);
            break;
        }

        if (old_rows != Rows) {
            resizeterm(Rows, Cols);
            old_rows = Rows;
        }
        move(0, 0);
        print_summary();
        attron(A_REVERSE);
        print_headings();
        attroff(A_REVERSE);

        for (i = 0; i < Rows - 8 && i < reaped->total; i++)
            print_details(reaped->stacks[i]);

        refresh();
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = Delay;
        tv.tv_usec = 0;
        if (select(STDOUT_FILENO, &readfds, NULL, NULL, &tv) > 0) {
            char c;
            if (read(STDIN_FILENO, &c, 1) != 1
            || (c == 'Q' || c == 'q'))
                break;
            set_sort_stuff(c);
        }
    // made zero by sigint_handler()
    } while (Delay);

    if (!Run_once) {
        if (is_tty)
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &Saved_tty);
        endwin();
    }
    procps_slabinfo_unref(&Slab_info);
    return rc;
}
