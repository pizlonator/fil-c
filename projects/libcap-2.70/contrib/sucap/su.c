/*
 * Originally based on an implementation of `su' by
 *
 *     Peter Orbaek  <poe@daimi.aau.dk>
 *
 * obtained circa 1997 from ftp://ftp.daimi.aau.dk/pub/linux/poe/
 *
 * Rewritten for Linux-PAM by Andrew G. Morgan <morgan@linux.kernel.org>
 * Modified by Andrey V. Savochkin <saw@msu.ru>
 * Modified for use with libcap by Andrew G. Morgan <morgan@kernel.org>
 */

/* #define PAM_DEBUG */

#include <sys/prctl.h>

/* non-root user of convenience to block signals */
#define TEMP_UID                  1

#ifndef PAM_APP_NAME
#define PAM_APP_NAME              "su"
#endif /* ndef PAM_APP_NAME */

#define DEFAULT_HOME              "/"
#define DEFAULT_SHELL             "/bin/bash"
#define SLEEP_TO_KILL_CHILDREN    3  /* seconds to wait after SIGTERM before
					SIGKILL */
#define SU_FAIL_DELAY     2000000    /* usec on authentication failure */

#define RHOST_UNKNOWN_NAME        ""     /* perhaps "[from.where?]" */
#define DEVICE_FILE_PREFIX        "/dev/"
#define WTMP_LOCK_TIMEOUT         3      /* in seconds */

#ifndef UT_IDSIZE
#define UT_IDSIZE 4            /* XXX - this is sizeof(struct utmp.ut_id) */
#endif

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/wait.h>
#include <utmp.h>
#include <ctype.h>
#include <stdarg.h>
#include <netdb.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <sys/capability.h>

#include <security/_pam_macros.h>

/* -------------------------------------------- */
/* ------ declarations ------------------------ */
/* -------------------------------------------- */

extern char **environ;
static pam_handle_t *pamh = NULL;

static int wait_for_child_caught=0;
static int need_job_control=0;
static int is_terminal = 0;
static struct termios stored_mode;        /* initial terminal mode settings */
static uid_t terminal_uid = (uid_t) -1;
static uid_t invoked_uid = (uid_t) -1;

/* -------------------------------------------- */
/* ------ some local (static) functions ------- */
/* -------------------------------------------- */

/*
 * We will attempt to transcribe the following env variables
 * independent of whether we keep the whole environment. Others will
 * be set elsewhere: either in modules; or after the identity of the
 * user is known.
 */

static const char *posix_env[] = {
    "LANG",
    "LC_COLLATE",
    "LC_CTYPE",
    "LC_MONETARY",
    "LC_NUMERIC",
    "TZ",
    NULL
};

/*
 * make_environment transcribes a selection of environment variables
 * from the invoking user.
 */
static int make_environment(int keep_env)
{
    const char *tmpe;
    int i;
    int retval;

    if (keep_env) {
	/* preserve the original environment */
	return pam_misc_paste_env(pamh, (const char * const *)environ);
    }

    /* we always transcribe some variables anyway */
    tmpe = getenv("TERM");
    if (tmpe == NULL) {
	tmpe = "dumb";
    }
    retval = pam_misc_setenv(pamh, "TERM", tmpe, 0);
    if (retval == PAM_SUCCESS) {
	retval = pam_misc_setenv(pamh, "PATH", "/bin:/usr/bin", 0);
    }
    if (retval != PAM_SUCCESS) {
	tmpe = NULL;
	D(("error setting environment variables"));
	return retval;
    }

    /* also propagate the POSIX specific ones */
    for (i=0; retval == PAM_SUCCESS && posix_env[i]; ++i) {
	tmpe = getenv(posix_env[i]);
	if (tmpe != NULL) {
	    retval = pam_misc_setenv(pamh, posix_env[i], tmpe, 0);
	}
    }
    tmpe = NULL;

    return retval;
}

/*
 * checkfds ensures that stdout and stderr filedescriptors are
 * defined. If all else fails, it directs them to /dev/null.
 */
static void checkfds(void)
{
    struct stat st;
    int fd;

    if (fstat(1, &st) == -1) {
        fd = open("/dev/null", O_WRONLY);
        if (fd == -1) goto badfds;
        if (fd != 1) {
            if (dup2(fd, 1) == -1) goto badfds;
            if (close(fd) == -1) goto badfds;
        }
    }
    if (fstat(2, &st) == -1) {
        fd = open("/dev/null", O_WRONLY);
        if (fd == -1) goto badfds;
        if (fd != 2) {
            if (dup2(fd, 2) == -1) goto badfds;
            if (close(fd) == -1) goto badfds;
        }
    }

    return;

badfds:
    perror("bad filedes");
    exit(1);
}

/*
 * store_terminal_modes captures the current state of the input
 * terminal. Calling this at the start of the program, we ensure we
 * can restore these default settings when su exits.
 */
static void store_terminal_modes(void)
{
    if (isatty(STDIN_FILENO)) {
	is_terminal = 1;
	if (tcgetattr(STDIN_FILENO, &stored_mode) != 0) {
	    fprintf(stderr, PAM_APP_NAME ": couldn't copy terminal mode");
	    exit(1);
	}
	return;
    }
    fprintf(stderr, PAM_APP_NAME ": must be run from a terminal\n");
    exit(1);
}

/*
 * restore_terminal_modes resets the terminal to the state it was in
 * when the program started.
 *
 * Returns:
 *   0     ok
 *   1     error
 */
static int restore_terminal_modes(void)
{
    if (is_terminal && tcsetattr(STDIN_FILENO, TCSAFLUSH, &stored_mode) != 0) {
	fprintf(stderr, PAM_APP_NAME ": cannot restore terminal mode: %s\n",
		strerror(errno));
	return 1;
    } else {
	return 0;
    }
}

/* ------ unexpected signals ------------------ */

struct sigaction old_int_act, old_quit_act, old_tstp_act, old_pipe_act;

/*
 * disable_terminal_signals attempts to make the process resistant to
 * being stopped - it helps ensure that the PAM stack can complete
 * session and auth failure logging etc.
 */
static void disable_terminal_signals(void)
{
    /*
     * Protect the process from dangerous terminal signals.
     * The protection is implemented via sigaction() because
     * the signals are sent regardless of the process' uid.
     */
    struct sigaction act;

    act.sa_handler = SIG_IGN;  /* ignore the signal */
    sigemptyset(&act.sa_mask); /* no signal blocking on handler
				  call needed */
    act.sa_flags = SA_RESTART; /* do not reset after first signal
				  arriving, restart interrupted
				  system calls if possible */
    sigaction(SIGINT, &act, &old_int_act);
    sigaction(SIGQUIT, &act, &old_quit_act);
    /*
     * Ignore SIGTSTP signals. Why? attacker could otherwise stop
     * a process and a. kill it, or b. wait for the system to
     * shutdown - either way, nothing appears in syslogs.
     */
    sigaction(SIGTSTP, &act, &old_tstp_act);
    /*
     * Ignore SIGPIPE. The parent `su' process may print something
     * on stderr. Killing of the process would be undesired.
     */
    sigaction(SIGPIPE, &act, &old_pipe_act);
}

static void enable_terminal_signals(void)
{
    sigaction(SIGINT, &old_int_act, NULL);
    sigaction(SIGQUIT, &old_quit_act, NULL);
    sigaction(SIGTSTP, &old_tstp_act, NULL);
    sigaction(SIGPIPE, &old_pipe_act, NULL);
}

/* ------ terminal ownership ------------------ */

/*
 * change_terminal_owner changes the ownership of STDIN if needed.
 * Returns:
 *   0     ok,
 *  -1     fatal error (continuing is impossible),
 *   1     non-fatal error.
 * In the case of an error "err_descr" is set to the error message
 * and "callname" to the name of the failed call.
 */
static int change_terminal_owner(uid_t uid, int is_login,
				 const char **callname, const char **err_descr)
{
    /* determine who owns the terminal line */
    if (is_terminal && is_login) {
	struct stat stat_buf;
	cap_t current, working;
	int status;
	cap_value_t cchown = CAP_CHOWN;

	if (fstat(STDIN_FILENO, &stat_buf) != 0) {
            *callname = "fstat to STDIN";
	    *err_descr = strerror(errno);
	    return -1;
	}

	current = cap_get_proc();
	working = cap_dup(current);
	cap_set_flag(working, CAP_EFFECTIVE, 1, &cchown, CAP_SET);
	status = cap_set_proc(working);
	cap_free(working);

	if (status != 0) {
	    *callname = "capset CHOWN";
	} else if ((status = fchown(STDIN_FILENO, uid, -1)) != 0) {
	    *callname = "fchown of STDIN";
	} else {
	    cap_set_proc(current);
	}
	cap_free(current);

	if (status != 0) {
	    *err_descr = strerror(errno);
	    return 1;
	}

	terminal_uid = stat_buf.st_uid;
    }
    return 0;
}

/*
 * restore_terminal_owner changes the terminal owner back to the value
 * it had when su was started.
 */
static void restore_terminal_owner(void)
{
    if (terminal_uid != (uid_t) -1) {
	cap_t current, working;
	int status;
	cap_value_t cchown = CAP_CHOWN;

	current = cap_get_proc();
	working = cap_dup(current);
	cap_set_flag(working, CAP_EFFECTIVE, 1, &cchown, CAP_SET);
	status = cap_set_proc(working);
	cap_free(working);

	if (status == 0) {
	    status = fchown(STDIN_FILENO, terminal_uid, -1);
	    cap_set_proc(current);
	}
	cap_free(current);

        if (status != 0) {
            openlog(PAM_APP_NAME, LOG_CONS|LOG_PERROR|LOG_PID, LOG_AUTHPRIV);
	    syslog(LOG_ALERT, "Terminal owner hasn\'t been restored: %s",
		   strerror(errno));
	    closelog();
        }
        terminal_uid = (uid_t) -1;
    }
}

/*
 * make_process_unkillable changes the uid of the process. TEMP_UID is
 * used for this temporary state.
 *
 * Returns:
 *   0     ok,
 *  -1     fatal error (continue of the work is impossible),
 *   1     non-fatal error.
 * In the case of an error "err_descr" is set to the error message
 * and "callname" to the name of the failed call.
 */
static int make_process_unkillable(const char **callname,
				   const char **err_descr)
{
    invoked_uid = getuid();
    if (invoked_uid == TEMP_UID) {
	/* no change needed */
	return 0;
    }

    if (cap_setuid(TEMP_UID) != 0) {
        *callname = "setuid";
	*err_descr = strerror(errno);
	return -1;
    }
    return 0;
}

/*
 * make_process_killable restores the invoking uid to the current
 * process.
 */
static void make_process_killable(void)
{
    (void) cap_setuid(invoked_uid);
}

/* ------ command line parser ----------------- */

static void usage(int exit_val)
{
    fprintf(stderr,"usage: su [-] [-h] [-c \"command\"] [username]\n");
    exit(exit_val);
}

/*
 * parse_command_line extracts the options from the command line
 * arguments.
 */
static void parse_command_line(int argc, char *argv[], int *is_login,
			       const char **user, const char **command)
{
    int username_present, command_present;

    *is_login = 0;
    *user = NULL;
    *command = NULL;
    username_present = command_present = 0;

    while ( --argc > 0 ) {
	const char *token;

	token = *++argv;
	if (*token == '-') {
	    switch (*++token) {
	    case '\0':             /* su as a login shell for the user */
		if (*is_login)
		    usage(1);
		*is_login = 1;
		break;
	    case 'c':
		if (command_present) {
		    usage(1);
		} else {               /* indicate we are running commands */
		    if (*++token != '\0') {
			command_present = 1;
			*command = token;
		    } else if (--argc > 0) {
			command_present = 1;
			*command = *++argv;
		    } else
			usage(1);
		}
		break;
	    case 'h':
		usage(0);
	    default:
		usage(1);
	    }
	} else {                       /* must be username */
	    if (username_present) {
		usage(1);
	    }
	    username_present = 1;
	    *user = *argv;
	}
    }

    if (!username_present) {
	fprintf(stderr, PAM_APP_NAME ": requires a username\n");
	usage(1);
    }
}

/*
 * This following contains code that waits for a child process to die.
 * It also chooses to intercept a couple of signals that it will
 * kindly pass on a SIGTERM to the child ;^). Waiting again for the
 * child to exit. If the child resists dying, it will SIGKILL it!
 */

static void wait_for_child_catch_sig(int ignore)
{
    wait_for_child_caught = 1;
}

static void prepare_for_job_control(int need_it)
{
    sigset_t ourset;

    (void) sigfillset(&ourset);
    if (sigprocmask(SIG_BLOCK, &ourset, NULL) != 0) {
	fprintf(stderr,"[trouble blocking signals]\n");
	wait_for_child_caught = 1;
	return;
    }
    need_job_control = need_it;
}

static int wait_for_child(pid_t child)
{
    int retval, status, exit_code;
    sigset_t ourset;

    exit_code = -1; /* no exit code yet, exit codes could be from 0 to 255 */
    if (child == -1) {
	return exit_code;
    }

    /*
     * set up signal handling
     */

    if (!wait_for_child_caught) {
	struct sigaction action, defaction;

	action.sa_handler = wait_for_child_catch_sig;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	defaction.sa_handler = SIG_DFL;
	sigemptyset(&defaction.sa_mask);
	defaction.sa_flags = 0;

	sigemptyset(&ourset);

	if (   sigaddset(&ourset, SIGTERM)
	    || sigaction(SIGTERM, &action, NULL)
	    || sigaddset(&ourset, SIGHUP)
	    || sigaction(SIGHUP, &action, NULL)
	    || sigaddset(&ourset, SIGALRM)          /* required by sleep(3) */
            || (need_job_control && sigaddset(&ourset, SIGTSTP))
            || (need_job_control && sigaction(SIGTSTP, &defaction, NULL))
            || (need_job_control && sigaddset(&ourset, SIGTTIN))
            || (need_job_control && sigaction(SIGTTIN, &defaction, NULL))
            || (need_job_control && sigaddset(&ourset, SIGTTOU))
            || (need_job_control && sigaction(SIGTTOU, &defaction, NULL))
	    || (need_job_control && sigaddset(&ourset, SIGCONT))
            || (need_job_control && sigaction(SIGCONT, &defaction, NULL))
	    || sigprocmask(SIG_UNBLOCK, &ourset, NULL)
	    ) {
	    fprintf(stderr,"[trouble setting signal intercept]\n");
	    wait_for_child_caught = 1;
	}

	/* application should be ready for receiving a SIGTERM/HUP now */
    }

    /*
     * This code waits for the process to actually die. If it stops,
     * then the parent attempts to mimic the behavior of the
     * child.. There is a slight bug in the code when the 'su'd user
     * attempts to restart the child independently of the parent --
     * the child dies.
     */
    while (!wait_for_child_caught) {
        /* parent waits for child */
	if ((retval = waitpid(child, &status, 0)) <= 0) {
            if (errno == EINTR) {
                continue;             /* recovering from a 'fg' */
	    }
            fprintf(stderr, "[error waiting child: %s]\n", strerror(errno));
            /*
             * Break the loop keeping exit_code undefined.
             * Do we have a chance for a successful wait() call
             * after kill()? (SAW)
             */
            wait_for_child_caught = 1;
            break;
        } else {
	    /* the child is terminated via exit() or a fatal signal */
	    if (WIFEXITED(status)) {
		exit_code = WEXITSTATUS(status);
	    } else {
		exit_code = 1;
	    }
	    break;
	}
    }

    if (wait_for_child_caught) {
	fprintf(stderr,"\nKilling shell...");
	kill(child, SIGTERM);
    }

    /*
     * do we need to wait for the child to catch up?
     */
    if (wait_for_child_caught) {
	sleep(SLEEP_TO_KILL_CHILDREN);
	kill(child, SIGKILL);
	fprintf(stderr, "killed\n");
    }

    /*
     * collect the zombie the shell was killed by ourself
     */
    if (exit_code == -1) {
	do {
	    retval = waitpid(child, &status, 0);
	} while (retval == -1 && errno == EINTR);
	if (retval == -1) {
	    fprintf(stderr, PAM_APP_NAME ": the final wait failed: %s\n",
		    strerror(errno));
	}
	if (WIFEXITED(status)) {
	    exit_code = WEXITSTATUS(status);
	} else {
	    exit_code = 1;
	}
    }

    return exit_code;
}


/*
 * Next some code that parses the spawned shell command line.
 */

static const char * const *build_shell_args(const char *pw_shell, int login,
					    const char *command)
{
    int use_default = 1;  /* flag to signal we should use the default shell */
    const char **args=NULL;             /* array of PATH+ARGS+NULL pointers */

    D(("called."));
    if (login) {
        command = NULL;                 /* command always ignored for login */
    }

    if (pw_shell && *pw_shell != '\0') {
        char *line;
        const char *tmp, *tmpb=NULL;
        int arg_no=0,i;

        /* first find the number of arguments */
        D(("non-null shell"));
        for (tmp=pw_shell; *tmp; ++arg_no) {

            /* skip leading spaces */
            while (isspace(*tmp))
                ++tmp;

            if (tmpb == NULL)               /* mark beginning token */
                tmpb = tmp;
            if (*tmp == '\0')               /* end of line with no token */
                break;

            /* skip token */
            while (*tmp && !isspace(*tmp))
                ++tmp;
        }

        /*
         * We disallow shells:
         *    - without a full specified path;
         *    - when we are not logging in and the #args != 1
         *                                         (unlikely a simple shell)
         */

        D(("shell so far = %s, arg_no = %d", tmpb, arg_no));
        if (tmpb != NULL && tmpb[0] == '/'    /* something (full path) */
            && ( login || arg_no == 1 )       /* login, or single arg shells */
            ) {

            use_default = 0;                  /* we will use this shell */
            D(("committed to using user's shell"));
            if (command) {
                arg_no += 2;                  /* will append "-c" "command" */
            }

            /* allocate an array of pointers long enough */

            D(("building array of size %d", 2+arg_no));
            args = (const char **) calloc(2+arg_no, sizeof(const char *));
            if (args == NULL)
                return NULL;
            /* get a string long enough for all the arguments */

            D(("an array of size %d chars", 2+strlen(tmpb)
                                   + ( command ? 4:0 )));
            line = (char *) malloc(2+strlen(tmpb)
                                   + ( command ? 4:0 ));
            if (line == NULL) {
                free(args);
                return NULL;
            }

            /* fill array - tmpb points to start of first non-space char */

            line[0] = '-';
            strcpy(line+1, tmpb);

            /* append " -c" to line? */
            if (command) {
                strcat(line, " -c");
            }

            D(("complete command: %s [+] %s", line, command));

            tmp = strtok(line, " \t");
            D(("command path=%s", line+1));
            args[0] = line+1;

            if (login) {               /* standard procedure for login shell */
                D(("argv[0]=%s", line));
                args[i=1] = line;
            } else {                 /* not a login shell -- for use with su */
                D(("argv[0]=%s", line+1));
                args[i=1] = line+1;
            }

            while ((tmp = strtok(NULL, " \t"))) {
                D(("adding argument %d: %s",i,tmp));
                args[++i] = tmp;
            }
            if (command) {
                D(("appending command [%s]", command));
                args[++i] = command;
            }
            D(("terminating args with NULL"));
            args[++i] = NULL;
            D(("list completed."));
        }
    }

    /* should we use the default shell instead of specific one? */

    if (use_default && !login) {
        int last_arg;

        D(("selecting default shell"));
        last_arg = command ? 5:3;

        args = (const char **) calloc(last_arg--, sizeof(const char *));
        if (args == NULL) {
            return NULL;
        }
        args[1] = DEFAULT_SHELL;      /* mapped to argv[0] (NOT login shell) */
        args[0] = args[1];            /* path to program */
        if (command) {
            args[2] = "-c";           /* should perform command and exit */
            args[3] = command;        /* the desired command */
        }
        args[last_arg] = NULL;        /* terminate list of args */
    }

    D(("returning arg list"));
    return (const char * const *) args;
}


/* ------ abnormal termination ---------------- */

static void exit_now(int exit_code, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (pamh != NULL)
	pam_end(pamh, exit_code ? PAM_ABORT:PAM_SUCCESS);

    /* USER's shell may have completely broken terminal settings
       restore the sane(?) initial conditions */
    restore_terminal_modes();

    exit(exit_code);
}

/* ------ PAM setup --------------------------- */

static struct pam_conv conv = {
    misc_conv,                   /* defined in <pam_misc/libmisc.h> */
    NULL
};

static void do_pam_init(const char *user, int is_login)
{
    int retval;

    retval = pam_start(PAM_APP_NAME, user, &conv, &pamh);
    if (retval != PAM_SUCCESS) {
	/*
	 * From my point of view failing of pam_start() means that
	 * pamh isn't a valid handler. Without a handler
	 * we couldn't call pam_strerror :-(   1998/03/29 (SAW)
	 */
	fprintf(stderr, PAM_APP_NAME ": pam_start failed with code %d\n",
		retval);
	exit(1);
    }

    /*
     * Fill in some blanks
     */

    retval = make_environment(!is_login);
    D(("made_environment returned: %s", pam_strerror(pamh, retval)));

    if (retval == PAM_SUCCESS && is_terminal) {
	const char *terminal = ttyname(STDIN_FILENO);
	if (terminal) {
	    retval = pam_set_item(pamh, PAM_TTY, (const void *)terminal);
	} else {
	    retval = PAM_PERM_DENIED;                /* how did we get here? */
	}
	terminal = NULL;
    }

    if (retval == PAM_SUCCESS && is_terminal) {
	const char *ruser = getlogin();      /* Who is running this program? */
	if (ruser) {
	    retval = pam_set_item(pamh, PAM_RUSER, (const void *)ruser);
	} else {
	    retval = PAM_PERM_DENIED;             /* must be known to system */
	}
	ruser = NULL;
    }

    if (retval == PAM_SUCCESS) {
	retval = pam_set_item(pamh, PAM_RHOST, (const void *)"localhost");
    }

    if (retval != PAM_SUCCESS) {
	exit_now(1, PAM_APP_NAME ": problem establishing environment\n");
    }

    /* have to pause on failure. At least this long (doubles..) */
    retval = pam_fail_delay(pamh, SU_FAIL_DELAY);
    if (retval != PAM_SUCCESS) {
	exit_now(1, PAM_APP_NAME ": problem initializing failure delay\n");
    }
}

/*
 * authenticate_user arranges for the PAM authentication stack to run.
 */
static int authenticate_user(cap_t all, int *retval, const char **place,
			     const char **err_descr)
{
    *place = "pre-auth cap_set_proc";
    if (cap_set_proc(all)) {
	D(("failed to raise all capabilities"));
	*err_descr = "cap_set_proc() failed";
	*retval = PAM_SUCCESS;
	return 1;
    }

    D(("attempt to authenticate user"));
    *place = "pam_authenticate";
    *retval = pam_authenticate(pamh, 0);
    return (*retval != PAM_SUCCESS);
}

/*
 * user_accounting confirms an authenticated user is permitted service.
 */
static int user_accounting(cap_t all, int *retval, const char **place,
			   const char **err_descr) {
    *place = "user_accounting";
    if (cap_set_proc(all)) {
	D(("failed to raise all capabilities"));
	*err_descr = "cap_set_proc() failed";
	return 1;
    }
    *place = "pam_acct_mgmt";
    *retval = pam_acct_mgmt(pamh, 0);
    return (*retval != PAM_SUCCESS);
}

/*
 * Find entry for this terminal (if there is one).
 * Utmp file should have been opened and rewinded for the call.
 *
 * XXX: the search should be more or less compatible with libc one.
 * The caller expects that pututline with the same arguments
 * will replace the found entry.
 */
static const struct utmp *find_utmp_entry(const char *ut_line,
					  const char *ut_id)
{
    struct utmp *u_tmp_p;

    while ((u_tmp_p = getutent()) != NULL)
	if ((u_tmp_p->ut_type == INIT_PROCESS ||
             u_tmp_p->ut_type == LOGIN_PROCESS ||
             u_tmp_p->ut_type == USER_PROCESS ||
             u_tmp_p->ut_type == DEAD_PROCESS) &&
            !strncmp(u_tmp_p->ut_id, ut_id, UT_IDSIZE) &&
            !strncmp(u_tmp_p->ut_line, ut_line, UT_LINESIZE))
                break;

    return u_tmp_p;
}

/*
 * Identify the terminal name and the abbreviation we will use.
 */
static void set_terminal_name(const char *terminal, char *ut_line, char *ut_id)
{
    memset(ut_line, 0, UT_LINESIZE);
    memset(ut_id, 0, UT_IDSIZE);

    /* set the terminal entry */
    if ( *terminal == '/' ) {     /* now deal with filenames */
	int o1, o2;

	o1 = strncmp(DEVICE_FILE_PREFIX, terminal, 5) ? 0 : 5;
	if (!strncmp("/dev/tty", terminal, 8)) {
	    o2 = 8;
	} else {
	    o2 = strlen(terminal) - sizeof(UT_IDSIZE);
	    if (o2 < 0)
		o2 = 0;
	}

	strncpy(ut_line, terminal + o1, UT_LINESIZE);
	strncpy(ut_id, terminal + o2, UT_IDSIZE);
    } else if (strchr(terminal, ':')) {  /* deal with X-based session */
	const char *suffix;

	suffix = strrchr(terminal,':');
	strncpy(ut_line, terminal, UT_LINESIZE);
	strncpy(ut_id, suffix, UT_IDSIZE);
    } else {	                         /* finally deal with weird terminals */
	strncpy(ut_line, terminal, UT_LINESIZE);
	ut_id[0] = '?';
	strncpy(ut_id + 1, terminal, UT_IDSIZE - 1);
    }
}

/*
 * Append an entry to wtmp. See utmp_open_session for the return convention.
 * Be careful: the function uses alarm().
 */

#define WWTMP_STATE_BEGINNING     0
#define WWTMP_STATE_FILE_OPENED   1
#define WWTMP_STATE_SIGACTION_SET 2
#define WWTMP_STATE_LOCK_TAKEN    3

static int write_wtmp(struct utmp *u_tmp_p, const char **callname,
		      const char **err_descr)
{
    int w_tmp_fd;
    struct flock w_lock;
    struct sigaction act1, act2;
    int state;
    int retval;

    state = WWTMP_STATE_BEGINNING;
    retval = 1;

    do {
        D(("writing to wtmp"));
        w_tmp_fd = open(_PATH_WTMP, O_APPEND|O_WRONLY);
        if (w_tmp_fd == -1) {
            *callname = "wtmp open";
            *err_descr = strerror(errno);
            break;
        }
        state = WWTMP_STATE_FILE_OPENED;

        /* prepare for blocking operation... */
        act1.sa_handler = SIG_DFL;
        sigemptyset(&act1.sa_mask);
        act1.sa_flags = 0;
        if (sigaction(SIGALRM, &act1, &act2) == -1) {
            *callname = "sigaction";
            *err_descr = strerror(errno);
            break;
        }
        alarm(WTMP_LOCK_TIMEOUT);
        state = WWTMP_STATE_SIGACTION_SET;

        /* now we try to lock this file-rcord exclusively; non-blocking */
        memset(&w_lock, 0, sizeof(w_lock));
        w_lock.l_type = F_WRLCK;
        w_lock.l_whence = SEEK_END;
        if (fcntl(w_tmp_fd, F_SETLK, &w_lock) < 0) {
            D(("locking %s failed.", _PATH_WTMP));
            *callname = "fcntl(F_SETLK)";
            *err_descr = strerror(errno);
            break;
        }
        alarm(0);
        sigaction(SIGALRM, &act2, NULL);
        state = WWTMP_STATE_LOCK_TAKEN;

        if (write(w_tmp_fd, u_tmp_p, sizeof(struct utmp)) != -1) {
            retval = 0;
	}
    } while(0); /* it's not a loop! */

    if (state >= WWTMP_STATE_LOCK_TAKEN) {
        w_lock.l_type = F_UNLCK;               /* unlock wtmp file */
        fcntl(w_tmp_fd, F_SETLK, &w_lock);
    }else if (state >= WWTMP_STATE_SIGACTION_SET) {
        alarm(0);
        sigaction(SIGALRM, &act2, NULL);
    }

    if (state >= WWTMP_STATE_FILE_OPENED) {
        close(w_tmp_fd);                       /* close wtmp file */
        D(("wtmp written"));
    }

    return retval;
}

/*
 * XXX - if this gets turned into a module, make this a
 * pam_data item. You should put the pid in the name so we can
 * "probably" nest calls more safely...
 */
struct utmp *login_stored_utmp=NULL;

/*
 * Returns:
 *   0     ok,
 *   1     non-fatal error
 *  -1     fatal error
 *  callname and err_descr will be set
 * Be careful: the function indirectly uses alarm().
 */
static int utmp_do_open_session(const char *user, const char *terminal,
				const char *rhost, pid_t pid,
				const char **place, const char **err_descr)
{
    struct utmp u_tmp;
    const struct utmp *u_tmp_p;
    char ut_line[UT_LINESIZE], ut_id[UT_IDSIZE];
    int retval;

    set_terminal_name(terminal, ut_line, ut_id);

    utmpname(_PATH_UTMP);
    setutent();                                           /* rewind file */
    u_tmp_p = find_utmp_entry(ut_line, ut_id);

    /* reset new entry */
    memset(&u_tmp, 0, sizeof(u_tmp));                     /* reset new entry */
    if (u_tmp_p == NULL) {
	D(("[NEW utmp]"));
    } else {
	D(("[OLD utmp]"));

	/*
	 * here, we make a record of the former entry. If the
	 * utmp_close_session code is attached to the same process,
	 * the wtmp will be replaced, otherwise we leave init to pick
	 * up the pieces.
	 */
	if (login_stored_utmp == NULL) {
	    login_stored_utmp = malloc(sizeof(struct utmp));
            if (login_stored_utmp == NULL) {
                *place = "malloc";
                *err_descr = "fail";
                endutent();
                return -1;
            }
	}
        memcpy(login_stored_utmp, u_tmp_p, sizeof(struct utmp));
    }

    /* we adjust the entry to reflect the current session */
    {
	strncpy(u_tmp.ut_line, ut_line, UT_LINESIZE);
	memset(ut_line, 0, UT_LINESIZE);
	strncpy(u_tmp.ut_id, ut_id, UT_IDSIZE);
	memset(ut_id, 0, UT_IDSIZE);
	strncpy(u_tmp.ut_user, user
		, sizeof(u_tmp.ut_user));
	strncpy(u_tmp.ut_host, rhost ? rhost : RHOST_UNKNOWN_NAME
		, sizeof(u_tmp.ut_host));

	/* try to fill the host address entry */
	if (rhost != NULL) {
	    struct hostent *hptr;

	    /* XXX: it isn't good to do DNS lookup here...  1998/05/29  SAW */
            hptr = gethostbyname(rhost);
	    if (hptr != NULL && hptr->h_addr_list) {
		memcpy(&u_tmp.ut_addr, hptr->h_addr_list[0]
		       , sizeof(u_tmp.ut_addr));
	    }
	}

	/* we fill in the remaining info */
	u_tmp.ut_type = USER_PROCESS;          /* a user process starting */
	u_tmp.ut_pid = pid;                    /* session identifier */
	u_tmp.ut_time = time(NULL);
    }

    setutent();                                /* rewind file (replace old) */
    pututline(&u_tmp);                         /* write it to utmp */
    endutent();                                /* close the file */

    retval = write_wtmp(&u_tmp, place, err_descr); /* write to wtmp file */
    memset(&u_tmp, 0, sizeof(u_tmp));          /* reset entry */

    return retval;
}

static int utmp_do_close_session(const char *terminal,
				 const char **place, const char **err_descr)
{
    struct utmp u_tmp;
    const struct utmp *u_tmp_p;
    char ut_line[UT_LINESIZE], ut_id[UT_IDSIZE];

    set_terminal_name(terminal, ut_line, ut_id);

    utmpname(_PATH_UTMP);
    setutent();                                              /* rewind file */

    /*
     * if there was a stored entry, return it to the utmp file, else
     * if there is a session to close, we close that
     */
    if (login_stored_utmp) {
	pututline(login_stored_utmp);

	memcpy(&u_tmp, login_stored_utmp, sizeof(u_tmp));
	u_tmp.ut_time = time(NULL);            /* a new time to restart */

        write_wtmp(&u_tmp, place, err_descr);

	memset(login_stored_utmp, 0, sizeof(u_tmp)); /* reset entry */
	free(login_stored_utmp);
    } else {
        u_tmp_p = find_utmp_entry(ut_line, ut_id);
        if (u_tmp_p != NULL) {
            memset(&u_tmp, 0, sizeof(u_tmp));
            strncpy(u_tmp.ut_line, ut_line, UT_LINESIZE);
            strncpy(u_tmp.ut_id, ut_id, UT_IDSIZE);
            memset(&u_tmp.ut_user, 0, sizeof(u_tmp.ut_user));
            memset(&u_tmp.ut_host, 0, sizeof(u_tmp.ut_host));
            u_tmp.ut_addr = 0;
            u_tmp.ut_type = DEAD_PROCESS;      /* `old' login process */
            u_tmp.ut_pid = 0;
            u_tmp.ut_time = time(NULL);
            setutent();                        /* rewind file (replace old) */
            pututline(&u_tmp);                 /* mark as dead */

            write_wtmp(&u_tmp, place, err_descr);
        }
    }

    /* clean up */
    memset(ut_line, 0, UT_LINESIZE);
    memset(ut_id, 0, UT_IDSIZE);

    endutent();                                /* close utmp file */
    memset(&u_tmp, 0, sizeof(u_tmp));          /* reset entry */

    return 0;
}

/*
 * Returns:
 *   0     ok,
 *   1     non-fatal error
 *  -1     fatal error
 * place and err_descr will be set
 * Be careful: the function indirectly uses alarm().
 */
static int utmp_open_session(pid_t pid, int *retval,
			     const char **place, const char **err_descr)
{
    const char *user, *terminal, *rhost;

    *retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
    if (*retval != PAM_SUCCESS) {
        return -1;
    }
    *retval = pam_get_item(pamh, PAM_TTY, (const void **)&terminal);
    if (retval != PAM_SUCCESS) {
        return -1;
    }
    *retval = pam_get_item(pamh, PAM_RHOST, (const void **)&rhost);
    if (retval != PAM_SUCCESS) {
        rhost = NULL;
    }

    return utmp_do_open_session(user, terminal, rhost, pid, place, err_descr);
}

static int utmp_close_session(const char **place, const char **err_descr)
{
    int retval;
    const char *terminal;

    retval = pam_get_item(pamh, PAM_TTY, (const void **)&terminal);
    if (retval != PAM_SUCCESS) {
        *place = "pam_get_item(PAM_TTY)";
        *err_descr = pam_strerror(pamh, retval);
        return -1;
    }

    return utmp_do_close_session(terminal, place, err_descr);
}

/*
 * set_credentials raises the process and PAM credentials.
 */
static int set_credentials(cap_t all, int login,
			   const char **user_p, uid_t *uid_p,
			   const char **pw_shell, int *retval,
			   const char **place, const char **err_descr)
{
    const char *user;
    char *shell;
    cap_value_t csetgid = CAP_SETGID;
    cap_t current;
    int status;
    struct passwd *pw;
    uid_t uid;

    D(("get user from pam"));
    *place = "set_credentials";
    *retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
    if (*retval != PAM_SUCCESS || user == NULL || *user == '\0') {
	D(("error identifying user from PAM."));
	*retval = PAM_USER_UNKNOWN;
	return 1;
    }
    *user_p = user;

    /*
     * Add the LOGNAME and HOME environment variables.
     */

    pw = getpwnam(user);
    if (pw == NULL || (user = x_strdup(pw->pw_name)) == NULL) {
	D(("failed to identify user"));
	*retval = PAM_USER_UNKNOWN;
	return 1;
    }

    uid = pw->pw_uid;
    if (uid == 0) {
	D(("user is superuser: %s", user));
	*retval = PAM_CRED_ERR;
	return 1;
    }
    *uid_p = uid;

    shell = x_strdup(pw->pw_shell);
    if (shell == NULL) {
	D(("user %s has no shell", user));
	*retval = PAM_CRED_ERR;
	return 1;
    }

    if (login) {
	/* set LOGNAME, HOME */
	if (pam_misc_setenv(pamh, "LOGNAME", user, 0) != PAM_SUCCESS) {
	    D(("failed to set LOGNAME"));
	    *retval = PAM_CRED_ERR;
	    return 1;
	}
    }

    /* bash requires these be set to the target user values */
    if (pam_misc_setenv(pamh, "HOME", pw->pw_dir, 0) != PAM_SUCCESS) {
	D(("failed to set HOME"));
	*retval = PAM_CRED_ERR;
	return 1;
    }
    if (pam_misc_setenv(pamh, "USER", user, 0) != PAM_SUCCESS) {
	D(("failed to set USER"));
	*retval = PAM_CRED_ERR;
	return 1;
    }

    current = cap_get_proc();
    cap_set_flag(current, CAP_EFFECTIVE, 1, &csetgid, CAP_SET);
    status = cap_set_proc(current);
    cap_free(current);
    if (status != 0) {
	*err_descr = "unable to raise CAP_SETGID";
	return 1;
    }

    /* initialize groups */
    if (initgroups(pw->pw_name, pw->pw_gid) != 0 || setgid(pw->pw_gid) != 0) {
	D(("failed to setgid etc"));
	*retval = PAM_PERM_DENIED;
	return 1;
    }
    *pw_shell = shell;

    pw = NULL;                                                  /* be tidy */

    D(("desired uid=%d", uid));

    /* assume user's identity - but preserve the permitted set */
    if (cap_setuid(uid) != 0) {
	D(("failed to setuid: %v", strerror(errno)));
	*retval = PAM_PERM_DENIED;
	return 1;
    }

    /*
     * Next, we call the PAM framework to add/enhance the credentials
     * of this user [it may change the user's home directory in the
     * pam_env, and add supplemental group memberships...].
     */
    D(("setting credentials"));
    if (cap_set_proc(all)) {
	D(("failed to raise all capabilities"));
	*retval = PAM_PERM_DENIED;
	return 1;
    }

    D(("calling pam_setcred to establish credentials"));
    *retval = pam_setcred(pamh, PAM_ESTABLISH_CRED);

    return (*retval != PAM_SUCCESS);
}

/*
 * open_session invokes the open session PAM stack.
 */
static int open_session(cap_t all, int *retval, const char **place,
			const char **err_descr)
{
    /* Open the su-session */
    *place = "pam_open_session";
    if (cap_set_proc(all)) {
	D(("failed to raise t_caps capabilities"));
	*err_descr = "capability setting failed";
	return 1;
    }
    *retval = pam_open_session(pamh, 0);     /* Must take care to close */
    if (*retval != PAM_SUCCESS) {
	return 1;
    }
    return 0;
}

/* ------ shell invoker ----------------------- */

static int launch_callback_fn(void *h)
{
    pam_handle_t *my_pamh = h;
    int retval;

    D(("pam_end"));
    retval = pam_end(my_pamh, PAM_SUCCESS | PAM_DATA_SILENT);
    pamh = NULL;
    if (retval != PAM_SUCCESS) {
	return -1;
    }

    /*
     * Restore a signal status: information if the signal is ignored
     * is inherited across exec() call.  (SAW)
     */
    enable_terminal_signals();

#ifdef PAM_DEBUG
    cap_iab_t iab = cap_iab_get_proc();
    char *text = cap_iab_to_text(iab);
    D(("iab = %s", text));
    cap_free(text);
    cap_free(iab);
    cap_t cap = cap_get_proc();
    text = cap_to_text(cap, NULL);
    D(("cap = %s", text));
    cap_free(text);
    cap_free(cap);
#endif

    D(("about to launch"));
    return 0;
}

/* Returns PAM_<STATUS>. */
static int perform_launch_and_cleanup(cap_t all, int is_login, const char *user,
				      const char *shell, const char *command)
{
    int status;
    const char *home;
    const char * const * shell_args;
    char * const * shell_env;
    cap_launch_t launcher;
    pid_t child;
    cap_iab_t iab;

    /*
     * Break up the shell command into a command and arguments
     */
    shell_args = build_shell_args(shell, is_login, command);
    if (shell_args == NULL) {
	D(("failed to compute shell arguments"));
	return PAM_SYSTEM_ERR;
    }

    home = pam_getenv(pamh, "HOME");
    if ( !home || home[0] == '\0' ) {
	fprintf(stderr, "setting home directory for %s to %s\n",
		user, DEFAULT_HOME);
	home = DEFAULT_HOME;
	if (pam_misc_setenv(pamh, "HOME", home, 0) != PAM_SUCCESS) {
	    D(("unable to set $HOME"));
	    fprintf(stderr,
		    "Warning: unable to set HOME environment variable\n");
	}
    }
    if (is_login) {
	if (chdir(home) && chdir(DEFAULT_HOME)) {
	    D(("failed to change directory"));
	    return PAM_SYSTEM_ERR;
	}
    }

    shell_env = pam_getenvlist(pamh);
    if (shell_env == NULL) {
	D(("failed to obtain environment for child"));
	return PAM_SYSTEM_ERR;
    }

    iab = cap_iab_get_proc();
    if (iab == NULL) {
	D(("failed to read IAB value of process"));
	return PAM_SYSTEM_ERR;
    }

    launcher = cap_new_launcher(shell_args[0],
				(const char * const *) &shell_args[1],
				(const char * const *) shell_env);
    if (launcher == NULL) {
	D(("failed to initialize launcher"));
	return PAM_SYSTEM_ERR;
    }
    cap_launcher_callback(launcher, launch_callback_fn);

    child = cap_launch(launcher, pamh);
    cap_free(launcher);

    if (cap_set_proc(all) != 0) {
	D(("failed to restore process capabilities"));
	return PAM_SYSTEM_ERR;
    }

    /* job control is off for login sessions */
    prepare_for_job_control(!is_login && command != NULL);

    if (cap_setuid(TEMP_UID) != 0) {
	fprintf(stderr, "[failed to change monitor UID=%d]\n", TEMP_UID);
    }

    /* wait for child to terminate */
    status = wait_for_child(child);
    if (status != 0) {
	D(("shell returned %d", status));
    }
    return status;
}

static void close_session(cap_t all)
{
    int retval;

    D(("session %p closing", pamh));
    if (cap_set_proc(all)) {
	fprintf(stderr, "WARNING: could not raise all caps\n");
    }
    retval = pam_close_session(pamh, 0);
    if (retval != PAM_SUCCESS) {
	fprintf(stderr, "WARNING: could not close session\n\t%s\n",
		pam_strerror(pamh,retval));
    }
}

/* -------------------------------------------- */
/* ------ the application itself -------------- */
/* -------------------------------------------- */

int main(int argc, char *argv[])
{
    int retcode, is_login, status;
    int retval, final_retval; /* PAM_xxx return values */
    const char *command, *shell;
    uid_t uid;
    const char *place = NULL, *err_descr = NULL;
    cap_t all, t_caps;
    const char *user;

    all = cap_get_proc();
    cap_fill(all, CAP_EFFECTIVE, CAP_PERMITTED);
    cap_clear_flag(all, CAP_INHERITABLE);

    checkfds();

    /*
     * Check whether stdin is a terminal and store terminal modes for later.
     */
    store_terminal_modes();

    /* ---------- parse the argument list and --------- */
    /* ------ initialize the Linux-PAM interface ------ */
    {
	parse_command_line(argc, argv, &is_login, &user, &command);
	place = "do_pam_init";
	do_pam_init(user, is_login);   /* call pam_start and set PAM items */
	user = NULL;                   /* transient until PAM_USER defined */
    }

    /*
     * Turn off terminal signals - this is to be sure that su gets a
     * chance to call pam_end() and restore the terminal modes in
     * spite of the frustrated user pressing Ctrl-C.
     */
    disable_terminal_signals();

    /*
     * Random exits from here are strictly prohibited :-) (SAW) AGM
     * achieves this with goto's and a single exit at the end of main.
     */
    status = 1;                       /* fake exit status of a child */
    err_descr = NULL;                 /* errors haven't happened */

    if (make_process_unkillable(&place, &err_descr) != 0) {
	goto su_exit;
    }

    if (authenticate_user(all, &retval, &place, &err_descr) != 0) {
	goto auth_exit;
    }

    /*
     * The user is valid, but should they have access at this
     * time?
     */
    if (user_accounting(all, &retval, &place, &err_descr) != 0) {
	goto auth_exit;
    }

    D(("su attempt is confirmed as authorized"));

    if (set_credentials(all, is_login, &user, &uid, &shell,
			&retval, &place, &err_descr) != 0) {
	D(("failed to set credentials"));
	goto auth_exit;
    }

    /*
     * ... setup terminal, ...
     */
    retcode = change_terminal_owner(uid, is_login, &place, &err_descr);
    if (retcode > 0) {
	fprintf(stderr, PAM_APP_NAME ": %s: %s\n", place, err_descr);
	err_descr = NULL; /* forget about the problem */
    } else if (retcode < 0) {
	D(("terminal owner to uid=%d change failed", uid));
	goto auth_exit;
    }

    /*
     * Here the IAB value is fixed and may differ from all's
     * Inheritable value. So synthesize what we need to proceed in the
     * child, for now, in this current process.
     */
    place = "preserving inheritable parts";
    t_caps = cap_get_proc();
    if (t_caps == NULL) {
	D(("failed to read capabilities"));
	err_descr = "capability read failed";
	goto delete_cred;
    }
    if (cap_fill(t_caps, CAP_EFFECTIVE, CAP_PERMITTED)) {
	D(("failed to fill effective bits"));
	err_descr = "capability fill failed";
	goto delete_cred;
    }

    /*
     * ... make [uw]tmp entries.
     */
    if (is_login) {
	/*
	 * Note: we use the parent pid as a session identifier for
	 * the logging.
	 */
	retcode = utmp_open_session(getpid(), &retval, &place, &err_descr);
	if (retcode > 0) {
	    fprintf(stderr, PAM_APP_NAME ": %s: %s\n", place, err_descr);
	    err_descr = NULL; /* forget about this non-critical problem */
	} else if (retcode < 0) {
	    goto delete_cred;
	}
    }

#ifdef PAM_DEBUG
    cap_iab_t iab = cap_iab_get_proc();
    char *text = cap_iab_to_text(iab);
    D(("pre-session open iab = %s", text));
    cap_free(text);
    cap_free(iab);
#endif

    if (open_session(t_caps, &retval, &place, &err_descr) != 0) {
	goto utmp_closer;
    }

    status = perform_launch_and_cleanup(all, is_login, user, shell, command);
    close_session(all);

utmp_closer:
    if (is_login) {
	/* do [uw]tmp cleanup */
	retcode = utmp_close_session(&place, &err_descr);
	if (retcode) {
	    fprintf(stderr, PAM_APP_NAME ": %s: %s\n", place, err_descr);
	}
    }

delete_cred:
    D(("delete credentials"));
    if (cap_set_proc(all)) {
	D(("failed to raise all capabilities"));
    }
    retcode = pam_setcred(pamh, PAM_DELETE_CRED);
    if (retcode != PAM_SUCCESS) {
	fprintf(stderr, "WARNING: could not delete credentials\n\t%s\n",
		pam_strerror(pamh, retcode));
    }

    D(("return terminal to local control"));
    restore_terminal_owner();

auth_exit:
    D(("for clean up we restore the launching user"));
    make_process_killable();

    D(("all done - closing down pam"));
    if (retval != PAM_SUCCESS) {      /* PAM has failed */
	fprintf(stderr, PAM_APP_NAME ": %s\n", pam_strerror(pamh, retval));
	final_retval = PAM_ABORT;
    } else if (err_descr != NULL) {   /* a system error has happened */
	fprintf(stderr, PAM_APP_NAME ": %s: %s\n", place, err_descr);
	final_retval = PAM_ABORT;
    } else {
	final_retval = PAM_SUCCESS;
    }
    (void) pam_end(pamh, final_retval);
    pamh = NULL;

    if (restore_terminal_modes() != 0 && !status) {
	status = 1;
    }

su_exit:
    if (status != 0) {
	perror(PAM_APP_NAME " failed");
    }
    exit(status);                 /* transparent exit */
}
