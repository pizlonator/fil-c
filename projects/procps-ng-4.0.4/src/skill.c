/*
 * skill.c - send a signal to process
 *
 * Copyright © 2009-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 1998-2002 Albert Cahalan
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "fileutils.h"
#include "signals.h"
#include "strutils.h"
#include "nls.h"
#include "xalloc.h"
#include "rpmatch.h"

#include "misc.h"
#include "pids.h"

#define DEFAULT_NICE 4

struct run_time_conf_t {
    int fast;
    int interactive;
    int verbose;
    int warnings;
    int noaction;
    int debugging;
};
static int tty_count, uid_count, cmd_count, pid_count, namespace_count;
static int *ttys;
static uid_t *uids;
static const char **cmds;
static int *pids;
static char **namespaces;
static int ns_pid;
static struct procps_ns match_namespaces;
static int ns_flags = 0x3f;

#define ENLIST(thing,addme) do{ \
if(thing##_count < 0 || (size_t)thing##_count >= INT_MAX / sizeof(*thing##s)) \
	xerrx(EXIT_FAILURE, _("integer overflow")); \
thing##s = xrealloc(thing##s, sizeof(*thing##s)*(thing##_count+1)); \
thing##s[thing##_count++] = addme; \
}while(0)

struct pids_info *Pids_info;

enum pids_item items[] = {
    PIDS_ID_PID,
    PIDS_ID_EUID,
    PIDS_ID_EUSER,
    PIDS_TTY,
    PIDS_TTY_NAME,
    PIDS_CMD};
enum rel_items {
    EU_PID, EU_EUID, EU_EUSER, EU_TTY, EU_TTYNAME, EU_CMD};

static int my_pid;

static int sig_or_pri;

enum {
    PROG_UNKNOWN,
    PROG_SKILL,
    PROG_SNICE
};
static int program = PROG_UNKNOWN;

static int parse_namespaces(char *optarg)
{
    char *ptr = optarg, *tmp;
    int len, id;

    ns_flags = 0;
    while (1) {
        if (strchr(ptr, ',') == NULL) {
            len = -1;
            tmp = strdup(ptr);
        } else {
            len = strchr(ptr, ',') - ptr;
            tmp = strndup(ptr, len);
        }

        id = procps_ns_get_id(tmp);
        if (id == -1) {
            fprintf(stderr, "%s is not a valid namespace\n", tmp);
            free(tmp);
            return 1;
        }
        ns_flags |= (1 << id);
        ENLIST(namespace, tmp);

        if (len == -1)
            break;

        ptr+= len + 1;
    }
    return 0;
}

static int match_intlist(const int value, const int len, int *list)
{
    int i;

    for(i=0; i<len; i++)
        if (list[i] == value)
            return 1;
    return 0;
}

static int match_strlist(const char *value, const int len, const char **list)
{
    int i;

    for(i=0; i<len; i++)
        if (strcmp(list[i], value) == 0)
            return 1;
    return 0;
}

static int match_ns(const int pid)
{
    struct procps_ns proc_ns;
    int found = 1;
    int i;

    if (procps_ns_read_pid(pid, &proc_ns) < 0)
        xerrx(EXIT_FAILURE,
              _("Unable to read process namespace information"));
    for (i = 0; i < PROCPS_NS_COUNT; i++) {
        if (ns_flags & (1 << i)) {
            if (proc_ns.ns[i] != match_namespaces.ns[i])
                found = 0;
        }
    }

    return found;
}

#define PIDS_GETINT(e) PIDS_VAL(EU_ ## e, s_int, stack, Pids_info)
#define PIDS_GETSTR(e) PIDS_VAL(EU_ ## e, str, stack, Pids_info)

static int ask_user(struct pids_stack *stack)
{
    char *buf=NULL;
    size_t len=0;

    fprintf(stderr, "%-8s %-8s %5d %-16.16s   ? ",
            PIDS_GETSTR(TTYNAME),
            PIDS_GETSTR(EUSER),
            PIDS_GETINT(PID),
            PIDS_GETSTR(CMD));
    fflush(stdout);
    if (getline(&buf, &len, stdin) == -1) {
        free(buf);
        return 0;
    }
    if (rpmatch(buf) < 1) {
        free(buf);
        return 0;
    }
    free(buf);
    return 1;
}

static void nice_or_kill(struct pids_stack *stack,
                         struct run_time_conf_t *run_time)
{
    int failed;

    if (run_time->interactive && !ask_user(stack))
        return;

    /* do the actual work */
    errno = 0;
    if (program == PROG_SKILL)
        failed = kill(PIDS_GETINT(PID), sig_or_pri);
    else
        failed = setpriority(PRIO_PROCESS, PIDS_GETINT(PID), sig_or_pri);
    if ((run_time->warnings && failed) || run_time->debugging || run_time->verbose) {
        fprintf(stderr, "%-8s %-8s %5d %-16.16s   ",
            PIDS_GETSTR(TTYNAME),
            PIDS_GETSTR(EUSER),
            PIDS_GETINT(PID),
            PIDS_GETSTR(CMD));
        perror("");
        return;
    }
    if (run_time->interactive)
        return;
    if (run_time->noaction) {
        printf("%d\n", PIDS_GETINT(PID));
        return;
    }
}

#undef PIDS_GETINT
#undef PIDS_GETSTR

/* debug function */
static void show_lists(void)
{
    int i;

    fprintf(stderr, "signal: %d\n", sig_or_pri);

    fprintf(stderr, "%d TTY: ", tty_count);
    if (ttys) {
        i = tty_count;
        while (i--) {
            fprintf(stderr, "%d,%d%c", (ttys[i] >> 8) & 0xff,
                ttys[i] & 0xff, i ? ' ' : '\n');
        }
    } else
        fprintf(stderr, "\n");

    fprintf(stderr, "%d UID: ", uid_count);
    if (uids) {
        i = uid_count;
        while (i--)
            fprintf(stderr, "%d%c", uids[i], i ? ' ' : '\n');
    } else
        fprintf(stderr, "\n");

    fprintf(stderr, "%d PID: ", pid_count);
    if (pids) {
        i = pid_count;
        while (i--)
            fprintf(stderr, "%d%c", pids[i], i ? ' ' : '\n');
    } else
        fprintf(stderr, "\n");

    fprintf(stderr, "%d CMD: ", cmd_count);
    if (cmds) {
        i = cmd_count;
        while (i--)
            fprintf(stderr, "%s%c", cmds[i], i ? ' ' : '\n');
    } else
        fprintf(stderr, "\n");
}

static void scan_procs(struct run_time_conf_t *run_time)
{
 #define PIDS_GETINT(e) PIDS_VAL(EU_ ## e, s_int, reap->stacks[i], Pids_info)
 #define PIDS_GETUNT(e) PIDS_VAL(EU_ ## e, u_int, reap->stacks[i], Pids_info)
 #define PIDS_GETSTR(e) PIDS_VAL(EU_ ## e, str, reap->stacks[i], Pids_info)
    struct pids_fetch *reap;
    int i, total_procs;

    if (procps_pids_new(&Pids_info, items, 6) < 0)
        xerrx(EXIT_FAILURE,
              _("Unable to create pid Pids_info structure"));
    if ((reap = procps_pids_reap(Pids_info, PIDS_FETCH_TASKS_ONLY)) == NULL)
        xerrx(EXIT_FAILURE,
              _("Unable to load process information"));

    total_procs = reap->counts->total;
    for (i=0; i < total_procs; i++) {
        if (PIDS_GETINT(PID) == my_pid || PIDS_GETINT(PID) == 0)
            continue;
	if (pids && !match_intlist(PIDS_GETINT(PID), pid_count, pids))
	    continue;
        if (uids && !match_intlist(PIDS_GETUNT(EUID), uid_count, (int *)uids))
            continue;
        if (ttys && !match_intlist(PIDS_GETINT(TTY), tty_count, ttys))
            continue;
        if (cmds && !match_strlist(PIDS_GETSTR(CMD), cmd_count, cmds))
            continue;
        if (namespaces && !match_ns(PIDS_GETINT(PID)))
            continue;
        nice_or_kill(reap->stacks[i], run_time);
    }

 #undef PIDS_GETINT
 #undef PIDS_GETUNT
 #undef PIDS_GETSTR
}

/* skill and snice help */
static void __attribute__ ((__noreturn__)) skillsnice_usage(FILE * out)
{
    fputs(USAGE_HEADER, out);

    if (program == PROG_SKILL) {
        fprintf(out,
            _(" %s [signal] [options] <expression>\n"),
            program_invocation_short_name);
    } else {
        fprintf(out,
            _(" %s [new priority] [options] <expression>\n"),
            program_invocation_short_name);
    }
    fputs(USAGE_OPTIONS, out);
    fputs(_(" -f, --fast         fast mode (not implemented)\n"), out);
    fputs(_(" -i, --interactive  interactive\n"), out);
    fputs(_(" -l, --list         list all signal names\n"), out);
    fputs(_(" -L, --table        list all signal names in a nice table\n"), out);
    fputs(_(" -n, --no-action    do not actually kill processes; just print what would happen\n"), out);
    fputs(_(" -v, --verbose      explain what is being done\n"), out);
    fputs(_(" -w, --warnings     enable warnings (not implemented)\n"), out);
    fputs(USAGE_SEPARATOR, out);
    fputs(_("Expression can be: terminal, user, pid, command.\n"
        "The options below may be used to ensure correct interpretation.\n"), out);
    fputs(_(" -c, --command <command>  expression is a command name\n"), out);
    fputs(_(" -p, --pid <pid>          expression is a process id number\n"), out);
    fputs(_(" -t, --tty <tty>          expression is a terminal\n"), out);
    fputs(_(" -u, --user <username>    expression is a username\n"), out);
    fputs(USAGE_SEPARATOR, out);
    fputs(_("Alternatively, expression can be:\n"), out);
    fputs(_(" --ns <pid>               match the processes that belong to the same\n"
        "                          namespace as <pid>\n"), out);
    fputs(_(" --nslist <ns,...>        list which namespaces will be considered for\n"
        "                          the --ns option; available namespaces are\n:"
            "                          ipc, mnt, net, pid, user, uts\n"), out);

    fputs(USAGE_SEPARATOR, out);
    fputs(USAGE_SEPARATOR, out);
    fputs(USAGE_HELP, out);
    fputs(USAGE_VERSION, out);
    if (program == PROG_SKILL) {
        fprintf(out,
            _("\n"
              "The default signal is TERM. Use -l or -L to list available signals.\n"
              "Particularly useful signals include HUP, INT, KILL, STOP, CONT, and 0.\n"
              "Alternate signals may be specified in three ways: -SIGKILL -KILL -9\n"));
        fprintf(out, USAGE_MAN_TAIL("skill(1)"));
    } else {
        fprintf(out,
            _("\n"
              "The default priority is +4. (snice +4 ...)\n"
              "Priority numbers range from +20 (slowest) to -20 (fastest).\n"
              "Negative priority numbers are restricted to administrative users.\n"));
        fprintf(out, USAGE_MAN_TAIL("snice(1)"));
    }
    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}


static int snice_prio_option(int *argc, char **argv)
{
    int i = 1, nargs = *argc;
    long prio = DEFAULT_NICE;

    while (i < nargs) {
        if ((argv[i][0] == '-' || argv[i][0] == '+')
            && isdigit(argv[i][1])) {
            prio = strtol_or_err(argv[i],
                         _("failed to parse argument"));
            if (prio < INT_MIN || INT_MAX < prio)
                xerrx(EXIT_FAILURE,
                     _("priority %lu out of range"), prio);
			memmove(argv + i, argv + i + 1,
				sizeof(char *) * (nargs - i));
            nargs--;
        } else
            i++;
    }
    *argc = nargs;
    return (int)prio;
}

static void parse_options(int argc,
                 char **argv, struct run_time_conf_t *run_time)
{
    int signo = -1;
    int prino = DEFAULT_NICE;
    int ch, i;

    enum {
        NS_OPTION = CHAR_MAX + 1,
        NSLIST_OPTION,
    };

    static const struct option longopts[] = {
        {"command", required_argument, NULL, 'c'},
        {"debug", no_argument, NULL, 'd'},
        {"fast", no_argument, NULL, 'f'},
        {"interactive", no_argument, NULL, 'i'},
        {"list", no_argument, NULL, 'l'},
        {"no-action", no_argument, NULL, 'n'},
        {"pid", required_argument, NULL, 'p'},
        {"table", no_argument, NULL, 'L'},
        {"tty", required_argument, NULL, 't'},
        {"user", required_argument, NULL, 'u'},
        {"ns", required_argument, NULL, NS_OPTION},
        {"nslist", required_argument, NULL, NSLIST_OPTION},
        {"verbose", no_argument, NULL, 'v'},
        {"warnings", no_argument, NULL, 'w'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {NULL, 0, NULL, 0}
    };

    if (argc < 2)
        skillsnice_usage(stderr);

    sig_or_pri = -1;

    if (program == PROG_SNICE)
        prino = snice_prio_option(&argc, argv);
    else if (program == PROG_SKILL) {
        signo = skill_sig_option(&argc, argv);
        if (-1 < signo)
            sig_or_pri = signo;
    }

    while ((ch =
        getopt_long(argc, argv, "c:dfilnp:Lt:u:vwhV", longopts,
                NULL)) != -1)
        switch (ch) {
        case 'c':
            ENLIST(cmd, optarg);
            break;
        case 'd':
            run_time->debugging = 1;
            break;
        case 'f':
            run_time->fast = 1;
            break;
        case 'i':
            run_time->interactive = 1;
            break;
        case 'l':
            unix_print_signals();
            exit(EXIT_SUCCESS);
        case 'n':
            run_time->noaction = 1;
            break;
        case 'p':
            ENLIST(pid,
                   strtol_or_err(optarg,
                         _("failed to parse argument")));
            break;
        case 'L':
            pretty_print_signals();
            exit(EXIT_SUCCESS);
        case 't':
            {
                struct stat sbuf;
                char path[32];
                snprintf(path, 32, "/dev/%s", optarg);
                if (stat(path, &sbuf) >= 0
                    && S_ISCHR(sbuf.st_mode)) {
                    ENLIST(tty, sbuf.st_rdev);
                }
            }
            break;
        case 'u':
            {
                struct passwd *passwd_data;
                passwd_data = getpwnam(optarg);
                if (passwd_data) {
                    ENLIST(uid, passwd_data->pw_uid);
                }
            }
            break;
        case NS_OPTION:
            ns_pid = atoi(optarg);
            if (ns_pid == 0) {
                xwarnx(_("invalid pid number %s"), optarg);
                skillsnice_usage(stderr);
            }
            if (procps_ns_read_pid(ns_pid, &match_namespaces) < 0) {
                xwarnx(_("error reading reference namespace "
                     "information"));
                skillsnice_usage(stderr);
            }

            break;
        case NSLIST_OPTION:
            if (parse_namespaces(optarg)) {
                xwarnx(_("invalid namespace list"));
                skillsnice_usage(stderr);
            }
            break;
        case 'v':
            run_time->verbose = 1;
            break;
        case 'w':
            run_time->warnings = 1;
            break;
        case 'h':
            skillsnice_usage(stdout);
        case 'V':
            fprintf(stdout, PROCPS_NG_VERSION);
            exit(EXIT_SUCCESS);
        default:
            skillsnice_usage(stderr);
        }

    argc -= optind;
    argv += optind;

    for (i = 0; i < argc; i++) {
        long num;
        char *end = NULL;
        errno = 0;
        num = strtol(argv[0], &end, 10);
        if (errno == 0 && argv[0] != end && end != NULL && *end == '\0') {
            ENLIST(pid, num);
        } else {
            ENLIST(cmd, argv[0]);
        }
        argv++;
    }

    /* No more arguments to process. Must sanity check. */
    if (!tty_count && !uid_count && !cmd_count && !pid_count && !ns_pid)
        xerrx(EXIT_FAILURE, _("no process selection criteria"));
    if ((run_time->fast | run_time->interactive | run_time->
         verbose | run_time->warnings | run_time->noaction) & ~1)
        xerrx(EXIT_FAILURE, _("general flags may not be repeated"));
    if (run_time->interactive
        && (run_time->verbose | run_time->fast | run_time->noaction))
        xerrx(EXIT_FAILURE, _("-i makes no sense with -v, -f, and -n"));
    if (run_time->verbose && (run_time->interactive | run_time->fast))
        xerrx(EXIT_FAILURE, _("-v makes no sense with -i and -f"));
    if (run_time->noaction) {
        program = PROG_SKILL;
        /* harmless */
        sig_or_pri = 0;
    }
    if (program == PROG_SNICE)
        sig_or_pri = prino;
    else if (sig_or_pri < 0)
        sig_or_pri = SIGTERM;
}

/* main body */
int main(int argc, char ** argv)
{
#ifdef HAVE_PROGRAM_INVOCATION_NAME
    program_invocation_name = program_invocation_short_name;
#endif
    struct run_time_conf_t run_time;
    memset(&run_time, 0, sizeof(struct run_time_conf_t));
    my_pid = getpid();

    if (strcmp(program_invocation_short_name, "skill") == 0 ||
         strcmp(program_invocation_short_name, "lt-skill") == 0)
        program = PROG_SKILL;
    else if (strcmp(program_invocation_short_name, "snice") == 0 ||
         strcmp(program_invocation_short_name, "lt-snice") == 0)
        program = PROG_SNICE;
#ifdef __CYGWIN__
    else if (strcmp(program_invocation_short_name, "prockill") == 0 ||
             strcmp(program_invocation_short_name, "lt-prockill") == 0)
        program = PROG_KILL;
#endif

    switch (program) {
    case PROG_SNICE:
    case PROG_SKILL:
        setpriority(PRIO_PROCESS, my_pid, -20);
        parse_options(argc, argv, &run_time);
        if (run_time.debugging)
            show_lists();
        scan_procs(&run_time);
        break;
    default:
        fprintf(stderr, _("skill: \"%s\" is not supported\n"),
            program_invocation_short_name);
        fprintf(stderr, USAGE_MAN_TAIL("skill(1)"));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
