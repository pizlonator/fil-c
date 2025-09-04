/* vi: set sw=4 ts=4: */
/*
 * Mini start-stop-daemon implementation(s) for busybox
 *
 * Written by Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>,
 * Adapted for busybox David Kimdon <dwhedon@gordian.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/*
This is how it is supposed to work:

start-stop-daemon [OPTIONS] [--start|--stop] [[--] ARGS]

One (only) of these must be given:
        -S,--start              Start
        -K,--stop               Stop
        -T,--status             Check for the existence of a process, return exitcode (since version 1.16.1)
                                0 - program is running
                                1 - program is not running and the pid file exists
                                3 - program is not running
                                4 - can't determine program status

Search for matching processes.
If --stop is given, stop all matching processes (by sending a signal).
If --start is given, start a new process unless a matching process was found.

Options controlling process matching
(if multiple conditions are specified, all must match):
        -u,--user USERNAME|UID  Only consider this user's processes
        -n,--name PROCESS_NAME  Look for processes by matching PROCESS_NAME
                                with comm field in /proc/$PID/stat.
                                Only basename is compared:
                                "ntpd" == "./ntpd" == "/path/to/ntpd".
[TODO: can PROCESS_NAME be a full pathname? Should we require full match then
with /proc/$PID/exe or argv[0] (comm can't be matched, it never contains path)]
        -x,--exec EXECUTABLE    Look for processes that were started with this
                                command in /proc/$PID/exe and /proc/$PID/cmdline
                                (/proc/$PID/cmdline is a bbox extension)
                                Unlike -n, we match against the full path:
                                "ntpd" != "./ntpd" != "/path/to/ntpd"
        -p,--pidfile PID_FILE   Look for processes with PID from this file
        --pid PID               Look for process with this pid (since version 1.17.6)
        --ppid PPID             Look for processes with parent pid (since version 1.17.7)

Options which are valid for --start only:
        -x,--exec EXECUTABLE    Program to run (1st arg of execvp).
                                If no -x, EXECUTABLE is taken from ARGS[0]
        -a,--startas NAME       argv[0] (defaults to EXECUTABLE)
        -b,--background         Put process into background
        -O,--output FILE        Redirect stdout and stderr to FILE when forcing the
                                daemon into the background (since version 1.20.6).
                                Requires --background and absolute pathname (tested with 1.21.22).
                                Uses O_CREAT|O_APPEND!
                                If execv fails, error message goes to FILE.
        -N,--nicelevel N        Add N to process' nice level
        -c,--chuid USER[:[GRP]] Change to specified user [and group]
        -m,--make-pidfile       Write PID to the pidfile
                                (both -m and -p must be given!)
        -P,--procsched policy:priority
                                This alters the process scheduler policy and priority of the
                                process before starting it (since version 1.15.0).  The
                                priority can be optionally specified by appending a :
                                followed by the value. The default priority is 0. The
                                currently supported policy values are other, fifo and rr.
        -r,--chroot DIR         Change directory and chroot to DIR before starting the
                                process. Please note that the pidfile is also written after
                                the chroot.
        -d,--chdir DIR          Change directory to DIR before starting the process. This is
                                done after the chroot if the -r|--chroot option is set.
                                When not specified, start-stop-daemon will change directory to the
                                root directory before starting the process.
                                ^^^^ Gentoo does not have the default chdir("/"). Debian does.
        Tested -S with 1.21.22:
        "start-stop-daemon -S -x /bin/pwd" is the minimum needed to run pwd.
        "start-stop-daemon -S -a /bin/pwd -n pwd" works too.
        "start-stop-daemon -S -a /bin/pwd" does NOT work.
        Earlier versions were less picky (which? Or is it only Gentoo's clone?)

Options which are valid for --stop only:
        -s,--signal SIG         Signal to send (default:TERM)
        -t,--test               Exit with status 0 if process is found
                                (we don't actually start or stop daemons)
        --remove-pidfile        Used when stopping a program that does not remove its own pid
                                file (since version 1.17.19). Requires -p PIDFILE?

Misc options:
        -o,--oknodo             Exit with status 0 if nothing is done
        -q,--quiet              Quiet
        -v,--verbose            Verbose
*/
//config:config START_STOP_DAEMON
//config:	bool "start-stop-daemon (12 kb)"
//config:	default y
//config:	help
//config:	start-stop-daemon is used to control the creation and
//config:	termination of system-level processes, usually the ones
//config:	started during the startup of the system.
//config:
//config:config FEATURE_START_STOP_DAEMON_LONG_OPTIONS
//config:	bool "Enable long options"
//config:	default y
//config:	depends on START_STOP_DAEMON && LONG_OPTS
//config:
//config:config FEATURE_START_STOP_DAEMON_FANCY
//config:	bool "Support additional arguments"
//config:	default y
//config:	depends on START_STOP_DAEMON
//config:	help
//config:	-o|--oknodo ignored since we exit with 0 anyway
//config:	-v|--verbose
//config:	-N|--nicelevel N

//applet:IF_START_STOP_DAEMON(APPLET_ODDNAME(start-stop-daemon, start_stop_daemon, BB_DIR_SBIN, BB_SUID_DROP, start_stop_daemon))
/* not NOEXEC: uses bb_common_bufsiz1 */

//kbuild:lib-$(CONFIG_START_STOP_DAEMON) += start_stop_daemon.o

//usage:#define start_stop_daemon_trivial_usage
//usage:       "-S|-K [OPTIONS] [-- ARGS]"
//usage:#define start_stop_daemon_full_usage "\n\n"
//usage:       "Search for matching processes, and then\n"
//usage:       "-S: start a process unless a matching process is found\n"
//usage:       "-K: stop all matching processes\n"
//usage:     "\nProcess matching:"
//usage:     "\n	-u USERNAME|UID	Match only this user's processes"
//usage:     "\n	-n NAME		Match processes with NAME"
//usage:     "\n			in comm field in /proc/PID/stat"
//usage:     "\n	-x EXECUTABLE	Match processes with this command"
//usage:     "\n			in /proc/PID/cmdline"
//usage:     "\n	-p FILE		Match a process with PID from FILE"
//usage:     "\n	All specified conditions must match"
//usage:     "\n-S only:"
//usage:     "\n	-x EXECUTABLE	Program to run"
//usage:     "\n	-a NAME		Zeroth argument"
//usage:     "\n	-b		Background"
//usage:     "\n	-O FILE		Append stdout and stderr to FILE"
//usage:	IF_FEATURE_START_STOP_DAEMON_FANCY(
//usage:     "\n	-N N		Change nice level"
//usage:	)
//usage:     "\n	-c USER[:[GRP]]	Change user/group"
//usage:     "\n	-d DIR		Change to DIR"
//usage:     "\n	-m		Write PID to pidfile specified by -p"
//usage:     "\n-K only:"
//usage:     "\n	-s SIG		Signal to send"
//usage:     "\n	-t		Match only, exit with 0 if found"
//usage:     "\nOther:"
//usage:	IF_FEATURE_START_STOP_DAEMON_FANCY(
//usage:     "\n	-o		Exit with status 0 if nothing is done"
//usage:     "\n	-v		Verbose"
//usage:     "\n	-q		Quiet"
//usage:	)

/* Override ENABLE_FEATURE_PIDFILE */
#define WANT_PIDFILE 1
#include "libbb.h"
#include "common_bufsiz.h"

struct pid_list {
	struct pid_list *next;
	pid_t pid;
};

enum {
	CTX_STOP       = (1 <<  0),
	CTX_START      = (1 <<  1),
	OPT_BACKGROUND = (1 <<  2), // -b
	OPT_QUIET      = (1 <<  3), // -q
	OPT_TEST       = (1 <<  4), // -t
	OPT_MAKEPID    = (1 <<  5), // -m
	OPT_a          = (1 <<  6), // -a
	OPT_n          = (1 <<  7), // -n
	OPT_s          = (1 <<  8), // -s
	OPT_u          = (1 <<  9), // -u
	OPT_c          = (1 << 10), // -c
	OPT_d          = (1 << 11), // -d
	OPT_x          = (1 << 12), // -x
	OPT_p          = (1 << 13), // -p
	OPT_OUTPUT     = (1 << 14), // -O
	OPT_OKNODO     = (1 << 15) * ENABLE_FEATURE_START_STOP_DAEMON_FANCY, // -o
	OPT_VERBOSE    = (1 << 16) * ENABLE_FEATURE_START_STOP_DAEMON_FANCY, // -v
	OPT_NICELEVEL  = (1 << 17) * ENABLE_FEATURE_START_STOP_DAEMON_FANCY, // -N
};
#define QUIET (option_mask32 & OPT_QUIET)
#define TEST  (option_mask32 & OPT_TEST)

struct globals {
	struct pid_list *found_procs;
	const char *userspec;
	const char *cmdname;
	const char *execname;
	const char *pidfile;
	char *execname_cmpbuf;
	unsigned execname_sizeof;
	int user_id;
	smallint signal_nr;
#ifdef OLDER_VERSION_OF_X
	struct stat execstat;
#endif
} FIX_ALIASING;
#define G (*(struct globals*)bb_common_bufsiz1)
#define userspec          (G.userspec            )
#define cmdname           (G.cmdname             )
#define execname          (G.execname            )
#define pidfile           (G.pidfile             )
#define user_id           (G.user_id             )
#define signal_nr         (G.signal_nr           )
#define INIT_G() do { \
	setup_common_bufsiz(); \
	user_id = -1; \
	signal_nr = 15; \
} while (0)

#ifdef OLDER_VERSION_OF_X
/* -x,--exec EXECUTABLE
 * Look for processes with matching /proc/$PID/exe.
 * Match is performed using device+inode.
 */
static int pid_is_exec(pid_t pid)
{
	struct stat st;
	char buf[sizeof("/proc/%u/exe") + sizeof(int)*3];

	sprintf(buf, "/proc/%u/exe", (unsigned)pid);
	if (stat(buf, &st) < 0)
		return 0;
	if (st.st_dev == G.execstat.st_dev
	 && st.st_ino == G.execstat.st_ino)
		return 1;
	return 0;
}
#else
static int pid_is_exec(pid_t pid)
{
	ssize_t bytes;
	char buf[sizeof("/proc/%u/cmdline") + sizeof(int)*3];
	char *procname, *exelink;
	int match;

	procname = buf + sprintf(buf, "/proc/%u/exe", (unsigned)pid) - 3;

	exelink = xmalloc_readlink(buf);
	match = (exelink && strcmp(execname, exelink) == 0);
	free(exelink);
	if (match)
		return match;

	strcpy(procname, "cmdline");
	bytes = open_read_close(buf, G.execname_cmpbuf, G.execname_sizeof);
	if (bytes > 0) {
		G.execname_cmpbuf[bytes] = '\0';
		return strcmp(execname, G.execname_cmpbuf) == 0;
	}
	return 0;
}
#endif

static int pid_is_name(pid_t pid)
{
	/* /proc/PID/stat is "PID (comm_15_bytes_max) ..." */
	char buf[32]; /* should be enough */
	char *p, *pe;

	sprintf(buf, "/proc/%u/stat", (unsigned)pid);
	if (open_read_close(buf, buf, sizeof(buf) - 1) < 0)
		return 0;
	buf[sizeof(buf) - 1] = '\0'; /* paranoia */
	p = strchr(buf, '(');
	if (!p)
		return 0;
	pe = strrchr(++p, ')');
	if (!pe)
		return 0;
	*pe = '\0';
	/* we require comm to match and to not be truncated */
	/* in Linux, if comm is 15 chars, it may be a truncated
	 * name, so we don't allow that to match */
	if (strlen(p) >= COMM_LEN - 1) /* COMM_LEN is 16 */
		return 0;
	return strcmp(p, cmdname) == 0;
}

static int pid_is_user(int pid)
{
	struct stat sb;
	char buf[sizeof("/proc/") + sizeof(int)*3];

	sprintf(buf, "/proc/%u", (unsigned)pid);
	if (stat(buf, &sb) != 0)
		return 0;
	return (sb.st_uid == (uid_t)user_id);
}

static void check(int pid)
{
	struct pid_list *p;

	if (execname && !pid_is_exec(pid)) {
		return;
	}
	if (cmdname && !pid_is_name(pid)) {
		return;
	}
	if (userspec && !pid_is_user(pid)) {
		return;
	}
	p = xmalloc(sizeof(*p));
	p->next = G.found_procs;
	p->pid = pid;
	G.found_procs = p;
}

static void do_pidfile(void)
{
	FILE *f;
	unsigned pid;

	f = fopen_for_read(pidfile);
	if (f) {
		if (fscanf(f, "%u", &pid) == 1)
			check(pid);
		fclose(f);
	} else if (errno != ENOENT)
		bb_perror_msg_and_die("open pidfile %s", pidfile);
}

static void do_procinit(void)
{
	DIR *procdir;
	struct dirent *entry;
	int pid;

	if (pidfile) {
		do_pidfile();
		return;
	}

	procdir = xopendir("/proc");

	pid = 0;
	while (1) {
		errno = 0; /* clear any previous error */
		entry = readdir(procdir);
// TODO: this check is too generic, it's better
// to check for exact errno(s) which mean that we got stale entry
		if (errno) /* Stale entry, process has died after opendir */
			continue;
		if (!entry) /* EOF, no more entries */
			break;
		pid = bb_strtou(entry->d_name, NULL, 10);
		if (errno) /* NaN */
			continue;
		check(pid);
	}
	closedir(procdir);
	if (!pid)
		bb_simple_error_msg_and_die("nothing in /proc - not mounted?");
}

static int do_stop(void)
{
	const char *what;
	struct pid_list *p;
	int killed = 0;

	if (cmdname) {
		if (ENABLE_FEATURE_CLEAN_UP) what = xstrdup(cmdname);
		if (!ENABLE_FEATURE_CLEAN_UP) what = cmdname;
	} else if (execname) {
		if (ENABLE_FEATURE_CLEAN_UP) what = xstrdup(execname);
		if (!ENABLE_FEATURE_CLEAN_UP) what = execname;
	} else if (pidfile) {
		what = xasprintf("process in pidfile '%s'", pidfile);
	} else if (userspec) {
		what = xasprintf("process(es) owned by '%s'", userspec);
	} else {
		bb_simple_error_msg_and_die("internal error, please report");
	}

	if (!G.found_procs) {
		if (!QUIET)
			printf("no %s found; none killed\n", what);
		killed = -1;
		goto ret;
	}
	for (p = G.found_procs; p; p = p->next) {
		if (kill(p->pid, TEST ? 0 : signal_nr) == 0) {
			killed++;
		} else {
			bb_perror_msg("warning: killing process %u", (unsigned)p->pid);
			p->pid = 0;
			if (TEST) {
				/* Example: -K --test --pidfile PIDFILE detected
				 * that PIDFILE's pid doesn't exist */
				killed = -1;
				goto ret;
			}
		}
	}
	if (!QUIET && killed) {
		printf("stopped %s (pid", what);
		for (p = G.found_procs; p; p = p->next)
			if (p->pid)
				printf(" %u", (unsigned)p->pid);
		puts(")");
	}
 ret:
	if (ENABLE_FEATURE_CLEAN_UP)
		free((char *)what);
	return killed;
}

#if ENABLE_FEATURE_START_STOP_DAEMON_LONG_OPTIONS
static const char start_stop_daemon_longopts[] ALIGN1 =
	"stop\0"         No_argument       "K"
	"start\0"        No_argument       "S"
	"background\0"   No_argument       "b"
	"quiet\0"        No_argument       "q"
	"test\0"         No_argument       "t"
	"make-pidfile\0" No_argument       "m"
	"output\0"       Required_argument "O"
# if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	"oknodo\0"       No_argument       "o"
	"verbose\0"      No_argument       "v"
	"nicelevel\0"    Required_argument "N"
# endif
	"startas\0"      Required_argument "a"
	"name\0"         Required_argument "n"
	"signal\0"       Required_argument "s"
	"user\0"         Required_argument "u"
	"chuid\0"        Required_argument "c"
	"chdir\0"        Required_argument "d"
	"exec\0"         Required_argument "x"
	"pidfile\0"      Required_argument "p"
# if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	"retry\0"        Required_argument "R"
# endif
	;
# define GETOPT32 getopt32long
# define LONGOPTS start_stop_daemon_longopts,
#else
# define GETOPT32 getopt32
# define LONGOPTS
#endif

int start_stop_daemon_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int start_stop_daemon_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opt;
	const char *signame;
	const char *startas = NULL;
	char *chuid;
	const char *chdir;
	const char *output = NULL;
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
//	const char *retry_arg = NULL;
//	int retries = -1;
	const char *opt_N;
#endif

	INIT_G();

	opt = GETOPT32(argv, "^"
		"KSbqtma:n:s:u:c:d:x:p:O:"
		IF_FEATURE_START_STOP_DAEMON_FANCY("ovN:R:")
			"\0"
			"K:S:K--S:S--K"
			/* -K or -S is required; they are mutually exclusive */
			":m?p"    /* -p is required if -m is given */
			":K?xpun" /* -xpun (at least one) is required if -K is given */
			/* (the above does not seem to be enforced by Gentoo, it does nothing
			 * if no matching is specified with -K, and it ignores ARGS
			 * - does not take ARGS[0] as program name to kill)
			 */
//			":S?xa"   /* -xa (at least one) is required if -S is given */
//Gentoo clone: "start-stop-daemon -S -- sleep 5" is a valid invocation
			IF_FEATURE_START_STOP_DAEMON_FANCY(":q-v") /* -q turns off -v */
			,
		LONGOPTS
		&startas, &cmdname, &signame, &userspec, &chuid, &chdir, &execname, &pidfile, &output
		IF_FEATURE_START_STOP_DAEMON_FANCY(,&opt_N)
		/* We accept and ignore -R <param> / --retry <param> */
		IF_FEATURE_START_STOP_DAEMON_FANCY(,NULL)
	);

//-O requires --background and absolute pathname (tested with 1.21.22).
//We don't bother requiring that (smaller code):
//#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
//	if ((opt & OPT_OUTPUT) && !(opt & OPT_BACKGROUND))
//		bb_show_usage();
//#endif

	if (opt & OPT_s) {
		signal_nr = get_signum(signame);
		if (signal_nr < 0) bb_show_usage();
	}

	//argc -= optind;
	argv += optind;
// ARGS contains zeroth arg if -x/-a is not given, else it starts with 1st arg.
// These will try to execute "[/bin/]sleep 5":
// "start-stop-daemon -S               -- sleep 5"
// "start-stop-daemon -S -x /bin/sleep -- 5"
// "start-stop-daemon -S -a sleep      -- 5"
// NB: -n option does _not_ behave in this way: this will try to execute "5":
// "start-stop-daemon -S -n sleep      -- 5"
	if (opt & CTX_START) {
		if (!execname) { /* -x is not given */
			execname = startas;
			if (!execname) { /* neither -x nor -a is given */
				execname = argv[0];
				if (!execname)
					bb_show_usage();
				argv++;
			}
		}
		if (!startas) /* -a is not given: use -x EXECUTABLE or argv[0] */
			startas = execname;
		*--argv = (char *)startas;
	}
	if (execname) {
		G.execname_sizeof = strlen(execname) + 1;
		G.execname_cmpbuf = xmalloc(G.execname_sizeof + 1);
	}
//	IF_FEATURE_START_STOP_DAEMON_FANCY(
//		if (retry_arg)
//			retries = xatoi_positive(retry_arg);
//	)
	if (userspec) {
		user_id = bb_strtou(userspec, NULL, 10);
		if (errno)
			user_id = xuname2uid(userspec);
	}

	/* Both start and stop need to know current processes */
	do_procinit();

	if (opt & CTX_STOP) {
		int i = do_stop();
		return (opt & OPT_OKNODO) ? 0 : (i <= 0);
	}

	/* else: CTX_START (-S). execname can't be NULL. */

	if (G.found_procs) {
		if (!QUIET)
			printf("%s is already running\n", execname);
		return !(opt & OPT_OKNODO);
	}

#ifdef OLDER_VERSION_OF_X
	if (execname)
		xstat(execname, &G.execstat);
#endif

	if (opt & OPT_BACKGROUND) {
		/* Daemons usually call bb_daemonize_or_rexec(), but SSD can do
		 * without: SSD is not itself a daemon, it _execs_ a daemon.
		 * The usual NOMMU problem of "child can't run indefinitely,
		 * it must exec" does not bite us: we exec anyway.
		 *
		 * bb_daemonize(DAEMON_DEVNULL_STDIO | DAEMON_CLOSE_EXTRA_FDS | DAEMON_DOUBLE_FORK)
		 * can be used on MMU systems, but use of vfork()
		 * is preferable since we want to create pidfile
		 * _before_ parent returns, and vfork() on Linux
		 * ensures that (by blocking parent until exec in the child).
		 */
		pid_t pid = xvfork();
		if (pid != 0) {
			/* Parent */
			/* why _exit? the child may have changed the stack,
			 * so "return 0" may do bad things
			 */
			_exit_SUCCESS();
		}
		/* Child */
		setsid(); /* detach from controlling tty */
		/* Redirect stdin to /dev/null, close extra FDs */
		/* Testcase: "start-stop-daemon -Sb -d /does/not/exist usleep 1" should not eat error message */
		bb_daemon_helper(DAEMON_DEVNULL_STDIN + DAEMON_CLOSE_EXTRA_FDS);
		if (!output)
			output = bb_dev_null; /* redirect output just before execv */
		/* On Linux, session leader can acquire ctty
		 * unknowingly, by opening a tty.
		 * Prevent this: stop being a session leader.
		 */
		pid = xvfork();
		if (pid != 0)
			_exit_SUCCESS(); /* Parent */
	}
	if (opt & OPT_MAKEPID) {
		/* User wants _us_ to make the pidfile */
		write_pidfile(pidfile);
	}
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	if (opt & OPT_NICELEVEL) {
		/* Set process priority (must be before OPT_c) */
		int prio = getpriority(PRIO_PROCESS, 0) + xatoi_range(opt_N, INT_MIN/2, INT_MAX/2);
		if (setpriority(PRIO_PROCESS, 0, prio) < 0) {
			bb_perror_msg_and_die("setpriority(%d)", prio);
		}
	}
#endif
	if (opt & OPT_c) {
		struct bb_uidgid_t ugid;
		parse_chown_usergroup_or_die(&ugid, chuid);
		if (ugid.uid != (uid_t) -1L) {
			struct passwd *pw = xgetpwuid(ugid.uid);
			if (ugid.gid != (gid_t) -1L)
				pw->pw_gid = ugid.gid;
			/* initgroups, setgid, setuid: */
			change_identity(pw);
		} else if (ugid.gid != (gid_t) -1L) {
			xsetgid(ugid.gid);
			setgroups(1, &ugid.gid);
		}
	}
	if (opt & OPT_d) {
		xchdir(chdir);
	}
	if (output) {
		int outfd = xopen(output, O_WRONLY | O_CREAT | O_APPEND);
		xmove_fd(outfd, STDOUT_FILENO);
		xdup2(STDOUT_FILENO, STDERR_FILENO);
		/* on execv error, the message goes to -O file. This is intended */
	}
	/* Try:
	 * strace -oLOG start-stop-daemon -S -x /bin/usleep -a qwerty 500000
	 * should exec "/bin/usleep", but argv[0] should be "qwerty":
	 */
	execvp(execname, argv);
	bb_perror_msg_and_die("can't execute '%s'", startas);
}
