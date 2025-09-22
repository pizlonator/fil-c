/*
 * pgrep/pkill -- utilities to filter the process table
 *
 * Copyright © 2009-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2013-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 2012      Roberto Polli <rpolli@babel.it>
 * Copyright © 2002-2007 Albert Cahalan
 * Copyright © 2000      Kjetil Torgrim Homme <kjetilho@ifi.uio.no>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <regex.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <time.h>

#ifdef ENABLE_PIDWAIT
#include <sys/epoll.h>
#ifndef HAVE_PIDFD_OPEN
#include <sys/syscall.h>
#endif /* !HAVE_PIDFD_OPEN */
#endif

/* EXIT_SUCCESS is 0 */
/* EXIT_FAILURE is 1 */
#define EXIT_USAGE 2
#define EXIT_FATAL 3
#define XALLOC_EXIT_CODE EXIT_FATAL

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "signals.h"
#include "xalloc.h"

#include "misc.h"
#include "pids.h"

enum pids_item Items[] = {
    PIDS_ID_PID,
    PIDS_ID_PPID,
    PIDS_ID_PGRP,
    PIDS_ID_EUID,
    PIDS_ID_RUID,
    PIDS_ID_RGID,
    PIDS_ID_SESSION,
    PIDS_ID_TGID,
    PIDS_TICS_BEGAN,
    PIDS_TTY_NAME,
    PIDS_CMD,
    PIDS_CMDLINE,
    PIDS_STATE,
    PIDS_TIME_ELAPSED,
    PIDS_CGROUP_V,
    PIDS_SIGCATCH
};
enum rel_items {
    EU_PID, EU_PPID, EU_PGRP, EU_EUID, EU_RUID, EU_RGID, EU_SESSION,
    EU_TGID, EU_STARTTIME, EU_TTYNAME, EU_CMD, EU_CMDLINE, EU_STA, EU_ELAPSED,
    EU_CGROUP, EU_SIGCATCH
};
#define grow_size(x) do { \
	if ((x) < 0 || (size_t)(x) >= INT_MAX / 5 / sizeof(struct el)) \
		xerrx(EXIT_FAILURE, _("integer overflow")); \
	(x) = (x) * 5 / 4 + 4; \
} while (0)

static enum {
    PGREP = 0,
    PKILL,
#ifdef ENABLE_PIDWAIT
    PIDWAIT,
#endif
} prog_mode;

struct el {
    long    num;
    char *    str;
};

/* User supplied arguments */

static int opt_full = 0;
static int opt_long = 0;
static int opt_longlong = 0;
static int opt_oldest = 0;
static int opt_older = 0;
static int opt_newest = 0;
static int opt_negate = 0;
static int opt_exact = 0;
static int opt_count = 0;
static int opt_signal = SIGTERM;
static int opt_lock = 0;
static int opt_case = 0;
static int opt_echo = 0;
static int opt_threads = 0;
static pid_t opt_ns_pid = 0;
static bool use_sigqueue = false;
static bool require_handler = false;
static union sigval sigval = {0};

static const char *opt_delim = "\n";
static struct el *opt_pgrp = NULL;
static struct el *opt_rgid = NULL;
static struct el *opt_pid = NULL;
static struct el *opt_ppid = NULL;
static struct el *opt_ignore_ancestors = NULL;
static struct el *opt_sid = NULL;
static struct el *opt_term = NULL;
static struct el *opt_euid = NULL;
static struct el *opt_ruid = NULL;
static struct el *opt_nslist = NULL;
static struct el *opt_cgroup = NULL;
static char *opt_pattern = NULL;
static char *opt_pidfile = NULL;
static char *opt_runstates = NULL;

/* by default, all namespaces will be checked */
static int ns_flags = 0x3f;

static int __attribute__ ((__noreturn__)) usage(int opt)
{
    int err = (opt == '?');
    FILE *fp = err ? stderr : stdout;

    fputs(USAGE_HEADER, fp);
    fprintf(fp, _(" %s [options] <pattern>\n"), program_invocation_short_name);
    fputs(USAGE_OPTIONS, fp);
    switch (prog_mode) {
    case PGREP:
        fputs(_(" -d, --delimiter <string>  specify output delimiter\n"),fp);
        fputs(_(" -l, --list-name           list PID and process name\n"),fp);
        fputs(_(" -a, --list-full           list PID and full command line\n"),fp);
        fputs(_(" -v, --inverse             negates the matching\n"),fp);
        fputs(_(" -w, --lightweight         list all TID\n"), fp);
        break;
    case PKILL:
        fputs(_(" -<sig>                    signal to send (either number or name)\n"), fp);
        fputs(_(" -H, --require-handler     match only if signal handler is present\n"), fp);
        fputs(_(" -q, --queue <value>       integer value to be sent with the signal\n"), fp);
        fputs(_(" -e, --echo                display what is killed\n"), fp);
        break;
#ifdef ENABLE_PIDWAIT
    case PIDWAIT:
        fputs(_(" -e, --echo                display PIDs before waiting\n"), fp);
        break;
#endif
    }
    fputs(_(" -c, --count               count of matching processes\n"), fp);
    fputs(_(" -f, --full                use full process name to match\n"), fp);
    fputs(_(" -g, --pgroup <PGID,...>   match listed process group IDs\n"), fp);
    fputs(_(" -G, --group <GID,...>     match real group IDs\n"), fp);
    fputs(_(" -i, --ignore-case         match case insensitively\n"), fp);
    fputs(_(" -n, --newest              select most recently started\n"), fp);
    fputs(_(" -o, --oldest              select least recently started\n"), fp);
    fputs(_(" -O, --older <seconds>     select where older than seconds\n"), fp);
    fputs(_(" -P, --parent <PPID,...>   match only child processes of the given parent\n"), fp);
    fputs(_(" -s, --session <SID,...>   match session IDs\n"), fp);
    fputs(_("     --signal <sig>        signal to send (either number or name)\n"), fp);
    fputs(_(" -t, --terminal <tty,...>  match by controlling terminal\n"), fp);
    fputs(_(" -u, --euid <ID,...>       match by effective IDs\n"), fp);
    fputs(_(" -U, --uid <ID,...>        match by real IDs\n"), fp);
    fputs(_(" -x, --exact               match exactly with the command name\n"), fp);
    fputs(_(" -F, --pidfile <file>      read PIDs from file\n"), fp);
    fputs(_(" -L, --logpidfile          fail if PID file is not locked\n"), fp);
    fputs(_(" -r, --runstates <state>   match runstates [D,S,Z,...]\n"), fp);
    fputs(_(" -A, --ignore-ancestors    exclude our ancestors from results\n"), fp);
    fputs(_(" --cgroup <grp,...>        match by cgroup v2 names\n"), fp);
    fputs(_(" --ns <PID>                match the processes that belong to the same\n"
        "                           namespace as <pid>\n"), fp);
    fputs(_(" --nslist <ns,...>         list which namespaces will be considered for\n"
        "                           the --ns option.\n"
        "                           Available namespaces: ipc, mnt, net, pid, user, uts\n"), fp);
    fputs(USAGE_SEPARATOR, fp);
    fputs(USAGE_HELP, fp);
    fputs(USAGE_VERSION, fp);
    fprintf(fp, USAGE_MAN_TAIL("pgrep(1)"));

    exit(fp == stderr ? EXIT_USAGE : EXIT_SUCCESS);
}

static struct el *get_our_ancestors(void)
{
#define PIDS_GETINT(e) PIDS_VAL(EU_##e, s_int, stack, info)
    struct el *list = NULL;
    int i = 0;
    int size = 0;
    int done = 0;
    pid_t search_pid = getpid();
    struct pids_stack *stack;

    while (!done) {
        struct pids_info *info = NULL;

        if (procps_pids_new(&info, Items, 16) < 0)
            xerrx(EXIT_FATAL, _("Unable to create pid info structure"));

        if (i == size) {
            grow_size(size);
            list = xrealloc(list, (1 + size) * sizeof(*list));
        }

        done = 1;
        while ((stack = procps_pids_get(info, PIDS_FETCH_TASKS_ONLY))) {
            if (PIDS_GETINT(PID) == search_pid) {
                list[++i].num = PIDS_GETINT(PPID);
                search_pid = list[i].num;
                done = 0;
                break;
            }
        }

        procps_pids_unref(&info);
    }

    if (i == 0) {
        free(list);
        list = NULL;
    } else {
        list[0].num = i;
    }
    return list;
#undef PIDS_GETINT
}

static struct el *split_list (const char *restrict str, int (*convert)(const char *, struct el *))
{
    char *copy;
    char *ptr;
    char *sep_pos;
    int i = 0;
    int size = 0;
    struct el *list = NULL;

    if (str[0] == '\0')
        return NULL;

    copy = xstrdup (str);
    ptr = copy;

    do {
        if (i == size) {
			grow_size(size);
            /* add 1 because slot zero is a count */
            list = xrealloc (list, (1 + size) * sizeof *list);
        }
        sep_pos = strchr (ptr, ',');
        if (sep_pos)
            *sep_pos = 0;
        /* Use ++i instead of i++ because slot zero is a count */
        if (list && !convert (ptr, &list[++i]))
            exit (EXIT_USAGE);
        if (sep_pos)
            ptr = sep_pos + 1;
    } while (sep_pos);

    free (copy);
    if (!i) {
        free (list);
        list = NULL;
    } else {
        list[0].num = i;
    }
    return list;
}

/* strict_atol returns a Boolean: TRUE if the input string
 * contains a plain number, FALSE if there are any non-digits. */
static int strict_atol (const char *restrict str, long *restrict value)
{
	long res = 0;
	long sign = 1;

    if (*str == '+')
        ++str;
    else if (*str == '-') {
        ++str;
        sign = -1;
    }

    for ( ; *str; ++str) {
        if (! isdigit (*str))
            return 0;
        if (res >= LONG_MAX / 10)
            return 0;
        res *= 10;
        if (res >= LONG_MAX - (*str - '0'))
            return 0;
        res += *str - '0';
    }
    *value = sign * res;
    return 1;
}

#include <sys/file.h>

/* We try a read lock. The daemon should have a write lock.
 * Seen using flock: FreeBSD code */
static int has_flock(int fd)
{
    return flock(fd, LOCK_SH|LOCK_NB)==-1 && errno==EWOULDBLOCK;
}

/* We try a read lock. The daemon should have a write lock.
 * Seen using fcntl: libslack */
static int has_fcntl(int fd)
{
    struct flock f;  /* seriously, struct flock is for a fnctl lock! */
    f.l_type = F_RDLCK;
    f.l_whence = SEEK_SET;
    f.l_start = 0;
    f.l_len = 0;
    return fcntl(fd,F_SETLK,&f)==-1 && (errno==EACCES || errno==EAGAIN);
}

static struct el *read_pidfile(void)
{
    char buf[12];
    int fd;
    struct stat sbuf;
    char *endp;
    int n, pid;
    struct el *list = NULL;

    fd = open(opt_pidfile, O_RDONLY|O_NOCTTY|O_NONBLOCK);
    if(fd<0)
        goto just_ret;
    if(fstat(fd,&sbuf) || !S_ISREG(sbuf.st_mode) || sbuf.st_size<1)
        goto out;
    /* type of lock, if any, is not standardized on Linux */
    if(opt_lock && !has_flock(fd) && !has_fcntl(fd))
        goto out;
    memset(buf,'\0',sizeof buf);
    n = read(fd,buf,sizeof buf-1);
    if (n<1)
        goto out;
    pid = strtoul(buf,&endp,10);
    if(endp<=buf || pid<1 )
        goto out;
    if(*endp && !isspace(*endp))
        goto out;
    list = xmalloc(2 * sizeof *list);
    list[0].num = 1;
    list[1].num = pid;
out:
    close(fd);
just_ret:
    return list;
}

static int conv_uid (const char *restrict name, struct el *restrict e)
{
    struct passwd *pwd;

    if (strict_atol (name, &e->num))
        return (1);

    pwd = getpwnam (name);
    if (pwd == NULL) {
        xwarnx(_("invalid user name: %s"), name);
        return 0;
    }
    e->num = pwd->pw_uid;
    return 1;
}


static int conv_gid (const char *restrict name, struct el *restrict e)
{
    struct group *grp;

    if (strict_atol (name, &e->num))
        return 1;

    grp = getgrnam (name);
    if (grp == NULL) {
        xwarnx(_("invalid group name: %s"), name);
        return 0;
    }
    e->num = grp->gr_gid;
    return 1;
}


static int conv_pgrp (const char *restrict name, struct el *restrict e)
{
    if (! strict_atol (name, &e->num)) {
        xwarnx(_("invalid process group: %s"), name);
        return 0;
    }
    if (e->num == 0)
        e->num = getpgrp ();
    return 1;
}


static int conv_sid (const char *restrict name, struct el *restrict e)
{
    if (! strict_atol (name, &e->num)) {
        xwarnx(_("invalid session id: %s"), name);
        return 0;
    }
    if (e->num == 0)
        e->num = getsid (0);
    return 1;
}


static int conv_num (const char *restrict name, struct el *restrict e)
{
    if (! strict_atol (name, &e->num)) {
        xwarnx(_("not a number: %s"), name);
        return 0;
    }
    return 1;
}


static int conv_str (const char *restrict name, struct el *restrict e)
{
    e->str = xstrdup (name);
    return 1;
}


static int conv_ns (const char *restrict name, struct el *restrict e)
{
    int rc = conv_str(name, e);
    int id;

    ns_flags = 0;
    id = procps_ns_get_id(name);
    if (id < 0)
        return 0;
    ns_flags |= (1 << id);

    return rc;
}

static int match_numlist (long value, const struct el *restrict list)
{
    int found = 0;
	if (list != NULL) {
        int i;
        for (i = list[0].num; i > 0; i--) {
			if (list[i].num == value) {
                found = 1;
				break;
			}
        }
    }
    return found;
}

static unsigned long long unhex (const char *restrict in)
{
    unsigned long long ret;
    char *rem;
    errno = 0;
    ret = strtoull(in, &rem, 16);
    if (errno || *rem != '\0') {
        xwarnx(_("not a hex string: %s"), in);
        return 0;
    }
    return ret;
}

static int match_signal_handler (const char *restrict sigcgt, const int signal)
{
    return sigcgt && (((1UL << (signal - 1)) & unhex(sigcgt)) != 0);
}

static int match_strlist (const char *restrict value, const struct el *restrict list)
{
    int found = 0;
	if (list != NULL) {
        int i;
        for (i = list[0].num; i > 0; i--) {
			if (! strcmp (list[i].str, value)) {
                found = 1;
				break;
			}
        }
    }
    return found;
}

static int match_ns (const int pid,
                     const struct procps_ns *match_ns)
{
    struct procps_ns proc_ns;
    int found = 1;
    int i;

    if (procps_ns_read_pid(pid, &proc_ns) < 0)
        xerrx(EXIT_FATAL,
              _("Unable to read process namespace information"));
    for (i = 0; i < PROCPS_NS_COUNT; i++) {
        if (ns_flags & (1 << i)) {
            if (proc_ns.ns[i] != match_ns->ns[i]) {
                found = 0;
                break;
            }
        }
    }
    return found;
}

static int cgroup_cmp(const char *restrict cgroup,
                        const char *restrict path)
{
    if (cgroup == NULL || path == NULL)
        return 1;
    // Cgroup v2 have 0::
    if (strncmp("0::", cgroup, 3) == 0) {
        return strcmp(cgroup+3, path);
    } //might try for cgroup v1 later
    return 1;
}


static int match_cgroup_list(char **values,
                        const struct el *restrict list)
{
	if (list != NULL && values != NULL) {
        int i, j;
        for (i = list[0].num; i > 0; i--) {
            for (j=0; values[j] && values[j][0]; j++) {
                if (! cgroup_cmp (values[j], list[i].str)) {
                    return 1;
                }
			}
        }
    }
    return 0;
}

static void output_numlist (const struct el *restrict list, int num)
{
    int i;
    const char *delim = opt_delim;
    for (i = 0; i < num; i++) {
        if(i+1==num)
            delim = "\n";
        printf ("%ld%s", list[i].num, delim);
    }
}

static void output_strlist (const struct el *restrict list, int num)
{
/* FIXME: escape codes */
    int i;
    const char *delim = opt_delim;
    for (i = 0; i < num; i++) {
        if(i+1==num)
            delim = "\n";
        printf ("%lu %s%s", list[i].num, list[i].str, delim);
    }
}

static regex_t * do_regcomp (void)
{
    regex_t *preg = NULL;

    if (opt_pattern) {
        char *re;
        char errbuf[256];
        int re_err;

        preg = xmalloc (sizeof (regex_t));
        if (opt_exact) {
            re = xmalloc (strlen (opt_pattern) + 5);
            sprintf (re, "^(%s)$", opt_pattern);
        } else {
            re = opt_pattern;
        }

        re_err = regcomp (preg, re, REG_EXTENDED | REG_NOSUB | opt_case);

        if (opt_exact) free(re);

        if (re_err) {
            regerror (re_err, preg, errbuf, sizeof(errbuf));
            xerrx(EXIT_USAGE, _("regex error: %s"), errbuf);
        }
    }
    return preg;
}

/*
 * SC_ARG_MAX used to return the maximum size a command line can be
 * however changes to the kernel mean this can be bigger than we can
 * alloc. Clamp it to 128kB like xargs and friends do
 * Should also not be smaller than POSIX_ARG_MAX which is 4096
 */
static size_t get_arg_max(void)
{
#define MIN_ARG_SIZE 4096u
#define MAX_ARG_SIZE (128u * 1024u)

    size_t val = sysconf(_SC_ARG_MAX);

    if (val < MIN_ARG_SIZE)
       val = MIN_ARG_SIZE;
    if (val > MAX_ARG_SIZE)
       val = MAX_ARG_SIZE;

    return val;
}

/*
 * Check if we have a long simple (non-regex) match
 * Returns true if the string:
 *  1) is longer than 15 characters
 *  2) Doesn't have | or [ which are used by regex
 * This is not an exhaustive list but catches most instances
 * It's only used to suppress the warning
 */
static bool is_long_match(const char *str)
{
    int i, len;

    if (str == NULL)
        return FALSE;
    if (15 >= (len = strlen(str)))
        return FALSE;
    for (i=0; i<len; i++)
        if (str[i] == '|' || str[i] == '[')
            return FALSE;
    return TRUE;
}
static struct el * select_procs (int *num)
{
#define PIDS_GETINT(e) PIDS_VAL(EU_ ## e, s_int, stack, info)
#define PIDS_GETUNT(e) PIDS_VAL(EU_ ## e, u_int, stack, info)
#define PIDS_GETULL(e) PIDS_VAL(EU_ ## e, ull_int, stack, info)
#define PIDS_GETSTR(e) PIDS_VAL(EU_ ## e, str, stack, info)
#define PIDS_GETSCH(e) PIDS_VAL(EU_ ## e, s_ch, stack, info)
#define PIDS_GETSTV(e) PIDS_VAL(EU_ ## e, strv, stack, info)
#define PIDS_GETFLT(e) PIDS_VAL(EU_ ## e, real, stack, info)
    struct pids_info *info=NULL;
    struct procps_ns nsp;
    struct pids_stack *stack;
    unsigned long long saved_start_time;      /* for new/old support */
    int saved_pid = 0;                        /* for new/old support */
    int matches = 0;
    int size = 0;
    regex_t *preg;
    pid_t myself = getpid();
    struct el *list = NULL;
    long cmdlen = get_arg_max() * sizeof(char);
    char *cmdline = xmalloc(cmdlen);
    char *cmdsearch = xmalloc(cmdlen);
    char *cmdoutput = xmalloc(cmdlen);
    char *task_cmdline;
    enum pids_fetch_type which;

    preg = do_regcomp();

    if (opt_newest) saved_start_time =  0ULL;
    else saved_start_time = ~0ULL;

    if (opt_newest) saved_pid = 0;
    if (opt_oldest) saved_pid = INT_MAX;
    if (opt_ns_pid && procps_ns_read_pid(opt_ns_pid, &nsp) < 0) {
        xerrx(EXIT_FATAL,
              _("Error reading reference namespace information\n"));
    }

    if (procps_pids_new(&info, Items, 16) < 0)
        xerrx(EXIT_FATAL,
              _("Unable to create pid info structure"));
    which = PIDS_FETCH_TASKS_ONLY;
    // pkill and pidwait don't support -w, but this is checked in getopt
    if (opt_threads)
        which = PIDS_FETCH_THREADS_TOO;

    while ((stack = procps_pids_get(info, which))) {
        int match = 1;

        if (PIDS_GETINT(PID) == myself)
            continue;
        else if (opt_ignore_ancestors && match_numlist(PIDS_GETINT(PID), opt_ignore_ancestors))
            continue;
        else if (opt_newest && PIDS_GETULL(STARTTIME) < saved_start_time)
            match = 0;
        else if (opt_oldest && PIDS_GETULL(STARTTIME) > saved_start_time)
            match = 0;
        else if (opt_ppid && ! match_numlist(PIDS_GETINT(PPID), opt_ppid))
            match = 0;
        else if (opt_pid && ! match_numlist (PIDS_GETINT(TGID), opt_pid))
            match = 0;
        else if (opt_pgrp && ! match_numlist (PIDS_GETINT(PGRP), opt_pgrp))
            match = 0;
        else if (opt_euid && ! match_numlist (PIDS_GETUNT(EUID), opt_euid))
            match = 0;
        else if (opt_ruid && ! match_numlist (PIDS_GETUNT(RUID), opt_ruid))
            match = 0;
        else if (opt_rgid && ! match_numlist (PIDS_GETUNT(RGID), opt_rgid))
            match = 0;
        else if (opt_sid && ! match_numlist (PIDS_GETINT(SESSION), opt_sid))
            match = 0;
        else if (opt_ns_pid && ! match_ns (PIDS_GETINT(PID), &nsp))
            match = 0;
	else if (opt_older && (int)PIDS_GETFLT(ELAPSED) < opt_older)
	    match = 0;
        else if (opt_term && ! match_strlist(PIDS_GETSTR(TTYNAME), opt_term))
            match = 0;
        else if (opt_runstates && ! strchr(opt_runstates, PIDS_GETSCH(STA)))
            match = 0;
        else if (opt_cgroup && ! match_cgroup_list (PIDS_GETSTV(CGROUP), opt_cgroup))
            match = 0;
        else if (require_handler && ! match_signal_handler (PIDS_GETSTR(SIGCATCH), opt_signal))
            match = 0;

        task_cmdline = PIDS_GETSTR(CMDLINE);

        if (opt_long || opt_longlong || (match && opt_pattern)) {
            if (opt_longlong)
                strncpy (cmdoutput, task_cmdline, cmdlen -1);
            else
                strncpy (cmdoutput, PIDS_GETSTR(CMD), cmdlen -1);
            cmdoutput[cmdlen - 1] = '\0';
        }

        if (match && opt_pattern) {
            if (opt_full)
                strncpy (cmdsearch, task_cmdline, cmdlen -1);
            else
                strncpy (cmdsearch, PIDS_GETSTR(CMD), cmdlen -1);
            cmdsearch[cmdlen - 1] = '\0';

            if (regexec (preg, cmdsearch, 0, NULL, 0) != 0)
                match = 0;
        }

        if (match ^ opt_negate) {    /* Exclusive OR is neat */
            if (opt_newest) {
                if (saved_start_time == PIDS_GETULL(STARTTIME) &&
                    saved_pid > PIDS_GETINT(PID))
                    continue;
                saved_start_time = PIDS_GETULL(STARTTIME);
                saved_pid = PIDS_GETINT(PID);
                matches = 0;
            }
            if (opt_oldest) {
                if (saved_start_time == PIDS_GETULL(STARTTIME) &&
                    saved_pid < PIDS_GETINT(PID))
                    continue;
                saved_start_time = PIDS_GETULL(STARTTIME);
                saved_pid = PIDS_GETINT(PID);
                matches = 0;
            }
            if (matches == size) {
				grow_size(size);
                list = xrealloc(list, size * sizeof *list);
            }
            if (list && (opt_long || opt_longlong || opt_echo)) {
                list[matches].num = PIDS_GETINT(PID);
                list[matches++].str = xstrdup (cmdoutput);
            } else if (list) {
                list[matches++].num = PIDS_GETINT(PID);
            } else {
                xerrx(EXIT_FATAL, _("internal error"));
            }
        }
    }
    procps_pids_unref(&info);
    free(cmdline);
    free(cmdsearch);
    free(cmdoutput);

    if (preg) {
        regfree(preg);
        free(preg);
    }

    *num = matches;

    if ((!matches) && (!opt_full) && is_long_match(opt_pattern))
        xwarnx(_("pattern that searches for process name longer than 15 characters will result in zero matches\n"
                 "Try `%s -f' option to match against the complete command line."),
               program_invocation_short_name);
    return list;
#undef PIDS_GETINT
#undef PIDS_GETUNT
#undef PIDS_GETULL
#undef PIDS_GETSTR
#undef PIDS_GETSTV
}

static int signal_option(int *argc, char **argv)
{
    int sig;
    int i;
    for (i = 1; i < *argc; i++) {
        if (argv[i][0] == '-') {
            sig = signal_name_to_number(argv[i] + 1);
            if (-1 < sig) {
                memmove(argv + i, argv + i + 1,
                    sizeof(char *) * (*argc - i));
                (*argc)--;
                return sig;
            }
        }
    }
    return -1;
}

#if defined(ENABLE_PIDWAIT) && !defined(HAVE_PIDFD_OPEN)
static int pidfd_open (pid_t pid, unsigned int flags)
{
	return syscall(__NR_pidfd_open, pid, flags);
}
#endif

static void parse_opts (int argc, char **argv)
{
    char opts[64] = "";
    int opt;
    int criteria_count = 0;

    enum {
        SIGNAL_OPTION = CHAR_MAX + 1,
        NS_OPTION,
        NSLIST_OPTION,
        CGROUP_OPTION,
    };
    static const struct option longopts[] = {
        {"signal", required_argument, NULL, SIGNAL_OPTION},
        {"ignore-ancestors", no_argument, NULL, 'A'},
        {"require-handler", no_argument, NULL, 'H'},
        {"count", no_argument, NULL, 'c'},
        {"cgroup", required_argument, NULL, CGROUP_OPTION},
        {"delimiter", required_argument, NULL, 'd'},
        {"list-name", no_argument, NULL, 'l'},
        {"list-full", no_argument, NULL, 'a'},
        {"full", no_argument, NULL, 'f'},
        {"pgroup", required_argument, NULL, 'g'},
        {"group", required_argument, NULL, 'G'},
        {"ignore-case", no_argument, NULL, 'i'},
        {"newest", no_argument, NULL, 'n'},
        {"oldest", no_argument, NULL, 'o'},
        {"older", required_argument, NULL, 'O'},
        {"parent", required_argument, NULL, 'P'},
        {"session", required_argument, NULL, 's'},
        {"terminal", required_argument, NULL, 't'},
        {"euid", required_argument, NULL, 'u'},
        {"uid", required_argument, NULL, 'U'},
        {"inverse", no_argument, NULL, 'v'},
        {"lightweight", no_argument, NULL, 'w'},
        {"exact", no_argument, NULL, 'x'},
        {"pidfile", required_argument, NULL, 'F'},
        {"logpidfile", no_argument, NULL, 'L'},
        {"echo", no_argument, NULL, 'e'},
        {"ns", required_argument, NULL, NS_OPTION},
        {"nslist", required_argument, NULL, NSLIST_OPTION},
        {"queue", required_argument, NULL, 'q'},
        {"runstates", required_argument, NULL, 'r'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {NULL, 0, NULL, 0}
    };


#ifdef ENABLE_PIDWAIT
    if (strcmp (program_invocation_short_name, "pidwait") == 0 ||
        strcmp (program_invocation_short_name, "lt-pidwait") == 0) {
        prog_mode = PIDWAIT;
        strcat (opts, "e");
    } else
#endif
    if (strcmp (program_invocation_short_name, "pkill") == 0 ||
        strcmp (program_invocation_short_name, "lt-pkill") == 0) {
        int sig;
        prog_mode = PKILL;
        sig = signal_option(&argc, argv);
        if (-1 < sig)
            opt_signal = sig;
	strcat (opts, "eq:");
    } else {
        strcat (opts, "lad:vw");
        prog_mode = PGREP;
    }

    strcat (opts, "LF:cfinoxP:O:AHg:s:u:U:G:t:r:?Vh");

    while ((opt = getopt_long (argc, argv, opts, longopts, NULL)) != -1) {
        switch (opt) {
        case SIGNAL_OPTION:
            opt_signal = signal_name_to_number (optarg);
            if (opt_signal == -1) {
                if (isdigit (optarg[0]))
                    opt_signal = atoi (optarg);
                else {
                    fprintf(stderr, _("Unknown signal \"%s\"."), optarg);
                    usage('?');
                }
            }
            break;
        case 'e':
            opt_echo = 1;
            break;
/*        case 'D':   / * FreeBSD: print info about non-matches for debugging * /
 *            break; */
        case 'F':   /* FreeBSD: the arg is a file containing a PID to match */
            free(opt_pidfile);
            opt_pidfile = xstrdup (optarg);
            ++criteria_count;
            break;
        case 'G':   /* Solaris: match rgid/rgroup */
            opt_rgid = split_list (optarg, conv_gid);
            if (opt_rgid == NULL)
                usage ('?');
            ++criteria_count;
            break;
/*        case 'I':   / * FreeBSD: require confirmation before killing * /
 *            break; */
/*        case 'J':   / * Solaris: match by project ID (name or number) * /
 *            break; */
        case 'L':   /* FreeBSD: fail if pidfile (see -F) not locked */
            opt_lock++;
            break;
/*        case 'M':   / * FreeBSD: specify core (OS crash dump) file * /
 *            break; */
/*        case 'N':   / * FreeBSD: specify alternate namelist file (for us, System.map -- but we don't need it) * /
 *            break; */
        case 'P':   /* Solaris: match by PPID */
            opt_ppid = split_list (optarg, conv_num);
            if (opt_ppid == NULL)
                usage ('?');
            ++criteria_count;
            break;
/*        case 'S':   / * FreeBSD: don't ignore the built-in kernel tasks * /
 *            break; */
/*        case 'T':   / * Solaris: match by "task ID" (probably not a Linux task) * /
 *            break; */
        case 'U':   /* Solaris: match by ruid/rgroup */
            opt_ruid = split_list (optarg, conv_uid);
            if (opt_ruid == NULL)
                usage ('?');
            ++criteria_count;
            break;
        case 'V':
            printf(PROCPS_NG_VERSION);
            exit(EXIT_SUCCESS);
/*        case 'c':   / * Solaris: match by contract ID * /
 *            break; */
        case 'c':
            opt_count = 1;
            break;
        case 'd':   /* Solaris: change the delimiter */
            opt_delim = xstrdup (optarg);
            break;
        case 'f':   /* Solaris: match full process name (as in "ps -f") */
            opt_full = 1;
            break;
        case 'g':   /* Solaris: match pgrp */
            opt_pgrp = split_list (optarg, conv_pgrp);
            if (opt_pgrp == NULL)
                usage ('?');
            ++criteria_count;
            break;
        case 'i':   /* FreeBSD: ignore case. OpenBSD: withdrawn. See -I. This sucks. */
            if (opt_case)
                usage (opt);
            opt_case = REG_ICASE;
            break;
/*        case 'j':   / * FreeBSD: restricted to the given jail ID * /
 *            break; */
        case 'l':   /* Solaris: long output format (pgrep only) Should require -f for beyond argv[0] maybe? */
            opt_long = 1;
            break;
        case 'a':
            opt_longlong = 1;
            break;
        case 'A':
            opt_ignore_ancestors = get_our_ancestors();
            break;
        case 'n':   /* Solaris: match only the newest */
            if (opt_oldest|opt_negate|opt_newest)
                usage ('?');
            opt_newest = 1;
            ++criteria_count;
            break;
        case 'o':   /* Solaris: match only the oldest */
            if (opt_oldest|opt_negate|opt_newest)
                usage ('?');
            opt_oldest = 1;
            ++criteria_count;
            break;
        case 'O':
            opt_older = atoi (optarg);
	    ++criteria_count;
	    break;
        case 's':   /* Solaris: match by session ID -- zero means self */
            opt_sid = split_list (optarg, conv_sid);
            if (opt_sid == NULL)
                usage ('?');
            ++criteria_count;
            break;
        case 't':   /* Solaris: match by tty */
            opt_term = split_list (optarg, conv_str);
            if (opt_term == NULL)
                usage ('?');
            ++criteria_count;
            break;
        case 'u':   /* Solaris: match by euid/egroup */
            opt_euid = split_list (optarg, conv_uid);
            if (opt_euid == NULL)
                usage ('?');
            ++criteria_count;
            break;
        case 'v':   /* Solaris: as in grep, invert the matching (uh... applied after selection I think) */
            if (opt_oldest|opt_negate|opt_newest)
                usage ('?');
            opt_negate = 1;
            break;
        case 'w':   // Linux: show threads (lightweight process) too
            opt_threads = 1;
            break;
        /* OpenBSD -x, being broken, does a plain string */
        case 'x':   /* Solaris: use ^(regexp)$ in place of regexp (FreeBSD too) */
            opt_exact = 1;
            break;
/*        case 'z':   / * Solaris: match by zone ID * /
 *            break; */
        case NS_OPTION:
            opt_ns_pid = atoi(optarg);
            if (opt_ns_pid == 0)
		case 'r': /* match by runstate */
			opt_runstates = xstrdup (optarg);
			++criteria_count;
			break;
        case NSLIST_OPTION:
            opt_nslist = split_list (optarg, conv_ns);
            if (opt_nslist == NULL)
                usage ('?');
            break;
        case 'q':
            sigval.sival_int = atoi(optarg);
            use_sigqueue = true;
            break;
        case CGROUP_OPTION:
            opt_cgroup = split_list (optarg, conv_str);
            if (opt_cgroup == NULL)
                usage ('?');
            ++criteria_count;
            break;
        case 'H':
            require_handler = true;
            ++criteria_count;
            break;
        case 'h':
        case '?':
            usage (opt);
            break;
        }
    }

    if(opt_lock && !opt_pidfile)
        xerrx(EXIT_USAGE, _("-L without -F makes no sense\n"
                     "Try `%s --help' for more information."),
                     program_invocation_short_name);

    if(opt_pidfile){
        opt_pid = read_pidfile();
        if(!opt_pid)
            xerrx(EXIT_FAILURE, _("pidfile not valid\n"
                         "Try `%s --help' for more information."),
                         program_invocation_short_name);
    }

    if (argc - optind == 1)
        opt_pattern = argv[optind];

    else if (argc - optind > 1)
        xerrx(EXIT_USAGE, _("only one pattern can be provided\n"
                     "Try `%s --help' for more information."),
                     program_invocation_short_name);
    else if (criteria_count == 0)
        xerrx(EXIT_USAGE, _("no matching criteria specified\n"
                     "Try `%s --help' for more information."),
                     program_invocation_short_name);
}

inline static int execute_kill(pid_t pid, int sig_num)
{
    if (use_sigqueue)
        return sigqueue(pid, sig_num, sigval);
    else
        return kill(pid, sig_num);
}

int main (int argc, char **argv)
{
    struct el *procs;
    int num;
    int i;
    int kill_count = 0;
#ifdef ENABLE_PIDWAIT
    int poll_count = 0;
    int wait_count = 0;
    int epollfd = epoll_create(1);
    struct epoll_event ev, events[32];
#endif

#ifdef HAVE_PROGRAM_INVOCATION_NAME
    program_invocation_name = program_invocation_short_name;
#endif
    setlocale (LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    atexit(close_stdout);

    parse_opts (argc, argv);

    procs = select_procs (&num);
    switch (prog_mode) {
    case PGREP:
        if (opt_count) {
            fprintf(stdout, "%d\n", num);
        } else {
            if (opt_long || opt_longlong)
                output_strlist (procs,num);
            else
                output_numlist (procs,num);
        }
        return !num;
    case PKILL:
        for (i = 0; i < num; i++) {
            if (execute_kill (procs[i].num, opt_signal) != -1) {
                if (opt_echo)
                    printf(_("%s killed (pid %lu)\n"), procs[i].str, procs[i].num);
                kill_count++;
                continue;
            }
            if (errno==ESRCH)
                 /* gone now, which is OK */
                continue;
            xwarn(_("killing pid %ld failed"), procs[i].num);
        }
        if (opt_count)
            fprintf(stdout, "%d\n", num);
        return !kill_count;
#ifdef ENABLE_PIDWAIT
    case PIDWAIT:
        if (opt_count)
            fprintf(stdout, "%d\n", num);

        for (i = 0; i < num; i++) {
            if (opt_echo)
                printf(_("waiting for %s (pid %lu)\n"), procs[i].str, procs[i].num);
            int pidfd = pidfd_open(procs[i].num, 0);
            if (pidfd == -1) {
		if (errno == ENOSYS)
		    xerrx(EXIT_FAILURE, _("pidfd_open() not implemented in Linux < 5.3"));
                /* ignore ESRCH, same as pkill */
                if (errno != ESRCH)
                    xwarn(_("opening pid %ld failed"), procs[i].num);
                continue;
            }
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = pidfd;
            if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pidfd, &ev) != -1)
                poll_count++;
        }

        while (wait_count < poll_count) {
            int ew = epoll_wait(epollfd, events, sizeof(events)/sizeof(events[0]), -1);
            if (ew == -1) {
                if (errno == EINTR)
                    continue;
                xwarn(_("epoll_wait failed"));
            }
            wait_count += ew;
        }

        return !wait_count;
#endif
    }
    /* Not sure if it is possible to get here */
    return -1;
}
