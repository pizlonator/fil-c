/*
 * pam_filter module
 *
 * written by Andrew Morgan <morgan@transmeta.com> with much help from
 * Richard Stevens' UNIX Network Programming book.
 */

#include "config.h"

#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <signal.h>

#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include "pam_filter.h"
#include "pam_inline.h"

/* ------ some tokens used for convenience throughout this file ------- */

#define FILTER_DEBUG     01
#define FILTER_RUN1      02
#define FILTER_RUN2      04
#define NEW_TERM        010
#define NON_TERM        020

/* -------------------------------------------------------------------- */

/* log errors */

#include <stdarg.h>

#define DEV_PTMX "/dev/ptmx"

static int
master (void)
{
    int fd;

    if ((fd = open(DEV_PTMX, O_RDWR)) >= 0) {
	return fd;
    }

    return -1;
}

static int process_args(pam_handle_t *pamh
			, int argc, const char **argv, const char *type
			, char ***evp, const char **filtername)
{
    int ctrl=0;

    while (argc-- > 0) {
	if (strcmp("debug",*argv) == 0) {
	    ctrl |= FILTER_DEBUG;
	} else if (strcmp("new_term",*argv) == 0) {
	    ctrl |= NEW_TERM;
	} else if (strcmp("non_term",*argv) == 0) {
	    ctrl |= NON_TERM;
	} else if (strcmp("run1",*argv) == 0) {
	    ctrl |= FILTER_RUN1;
	    if (argc <= 0) {
		pam_syslog(pamh, LOG_ERR, "no run filter supplied");
	    } else
		break;
	} else if (strcmp("run2",*argv) == 0) {
	    ctrl |= FILTER_RUN2;
	    if (argc <= 0) {
		pam_syslog(pamh, LOG_ERR, "no run filter supplied");
	    } else
		break;
	} else {
	    pam_syslog(pamh, LOG_ERR, "unrecognized option: %s", *argv);
	}
	++argv;                   /* step along list */
    }

    if (argc < 0) {
	/* there was no reference to a filter */
	*filtername = NULL;
	*evp = NULL;
    } else {
	char **levp;
	char *p;
	const char *user = NULL;
	const void *tmp;
	int i, retval;
	size_t size;

	*filtername = *++argv;
	if (ctrl & FILTER_DEBUG) {
	    pam_syslog(pamh, LOG_DEBUG, "will run filter %s", *filtername);
	}

	levp = malloc(5*sizeof(char *));
	if (levp == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "no memory for environment of filter");
	    return -1;
	}

	/* the "ARGS" variable */

#define ARGS_NAME      "ARGS="
#define ARGS_OFFSET    (sizeof(ARGS_NAME) - 1)

	size = sizeof(ARGS_NAME);

	for (i=0; i<argc; ++i) {
	    size += strlen(argv[i]) + (i != 0);
	}

	levp[0] = malloc(size);
	if (levp[0] == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "no memory for filter arguments");
	    free(levp);
	    return -1;
	}

	p = stpcpy(levp[0], ARGS_NAME);
	for (i=0; i<argc; ++i) {
	    if (i)
		p = stpcpy(p, " ");
	    p = stpcpy(p, argv[i]);
	}

	/* the "SERVICE" variable */

	retval = pam_get_item(pamh, PAM_SERVICE, &tmp);
	if (retval != PAM_SUCCESS || tmp == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "service name not found");
	    free(levp[0]);
	    free(levp);
	    return -1;
	}

	if ((levp[1] = pam_asprintf("SERVICE=%s", (const char *) tmp)) == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "no memory for service name");
	    free(levp[0]);
	    free(levp);
	    return -1;
	}

	/* the "USER" variable */

	if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS) {
	    user = "<unknown>";
	}

	if ((levp[2] = pam_asprintf("USER=%s", user)) == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "no memory for user's name");
	    free(levp[1]);
	    free(levp[0]);
	    free(levp);
	    return -1;
	}

	/* the "USER" variable */

	if ((levp[3] = pam_asprintf("TYPE=%s", type)) == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "no memory for type");
	    free(levp[2]);
	    free(levp[1]);
	    free(levp[0]);
	    free(levp);
	    return -1;
	}

	levp[4] = NULL;	                     /* end list */

	*evp = levp;
    }

    if ((ctrl & FILTER_DEBUG) && *filtername) {
	char **e;

	pam_syslog(pamh, LOG_DEBUG, "filter[%s]: %s", type, *filtername);
	pam_syslog(pamh, LOG_DEBUG, "environment:");
	for (e=*evp; e && *e; ++e) {
	    pam_syslog(pamh, LOG_DEBUG, "  %s", *e);
	}
    }

    return ctrl;
}

static void free_evp(char *evp[])
{
    int i;

    if (evp)
	for (i=0; i<4; ++i) {
	    free(evp[i]);
	}
    free(evp);
}

static int
set_filter (pam_handle_t *pamh, int flags UNUSED, int ctrl,
	    char * const evp[], const char *filtername)
{
    int status=-1;
    char* terminal = NULL;
    struct termios stored_mode;           /* initial terminal mode settings */
    int fd[2], child=0, child2=0, aterminal;

    if (filtername == NULL || *filtername != '/') {
	pam_syslog(pamh, LOG_ERR,
		   "filtername not permitted; full pathname required");
	return PAM_ABORT;
    }

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
	aterminal = 0;
    } else {
	aterminal = 1;
    }

    if (aterminal) {

	/* open the master pseudo terminal */

	fd[0] = master();
	if (fd[0] < 0) {
	    pam_syslog(pamh, LOG_CRIT, "no master terminal");
	    return PAM_AUTH_ERR;
	}

	/* set terminal into raw mode.. remember old mode so that we can
	   revert to it after the child has quit. */

	/* this is termios terminal handling... */

	if ( tcgetattr(STDIN_FILENO, &stored_mode) < 0 ) {
	    pam_syslog(pamh, LOG_CRIT, "couldn't copy terminal mode: %m");
	    /* in trouble, so close down */
	    close(fd[0]);
	    return PAM_ABORT;
	} else {
	    struct termios t_mode = stored_mode;

	    t_mode.c_iflag = 0;            /* no input control */
	    t_mode.c_oflag &= ~OPOST;      /* no output post processing */

	    /* no signals, canonical input, echoing, upper/lower output */
#ifdef XCASE
	    t_mode.c_lflag &= ~(XCASE);
#endif
	    t_mode.c_lflag &= ~(ISIG|ICANON|ECHO);
	    t_mode.c_cflag &= ~(CSIZE|PARENB);  /* no parity */
	    t_mode.c_cflag |= CS8;              /* 8 bit chars */

	    t_mode.c_cc[VMIN] = 1; /* number of chars to satisfy a read */
	    t_mode.c_cc[VTIME] = 0;          /* 0/10th second for chars */

	    if ( tcsetattr(STDIN_FILENO, TCSAFLUSH, &t_mode) < 0 ) {
		pam_syslog(pamh, LOG_ERR,
			   "couldn't put terminal in RAW mode: %m");
		close(fd[0]);
		return PAM_ABORT;
	    }

	    /*
	     * NOTE: Unlike the stream socket case here the child
	     * opens the slave terminal as fd[1] *after* the fork...
	     */
	}
    } else {

	/*
	 * not a terminal line so just open a stream socket fd[0-1]
	 * both set...
	 */

	if ( socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0 ) {
	    pam_syslog(pamh, LOG_ERR, "couldn't open a stream pipe: %m");
	    return PAM_ABORT;
	}
    }

    /* start child process */

    if ( (child = fork()) < 0 ) {

	pam_syslog(pamh, LOG_ERR, "first fork failed: %m");
	if (aterminal) {
		(void) tcsetattr(STDIN_FILENO, TCSAFLUSH, &stored_mode);
		close(fd[0]);
	} else {
		/* Socket pair */
		close(fd[0]);
		close(fd[1]);
	}

	return PAM_AUTH_ERR;
    }

    if ( child == 0 ) {                  /* child process *is* application */

	if (aterminal) {

	    /* close the controlling tty */
	    int t = open("/dev/tty",O_RDWR);
	    if (t >= 0) {
		(void) ioctl(t, TIOCNOTTY, NULL);
		close(t);
	    }

	    /* make this process its own process leader */
	    if (setsid() == -1) {
		pam_syslog(pamh, LOG_ERR,
			   "child cannot become new session: %m");
		close(fd[0]);
		return PAM_ABORT;
	    }

	    /* grant slave terminal */
	    if (grantpt (fd[0]) < 0) {
		pam_syslog(pamh, LOG_ERR, "Cannot grant access to slave terminal");
		close(fd[0]);
		return PAM_ABORT;
	    }

	    /* unlock slave terminal */
	    if (unlockpt (fd[0]) < 0) {
		pam_syslog(pamh, LOG_ERR, "Cannot unlock slave terminal");
		close(fd[0]);
		return PAM_ABORT;
	    }

	    /* find slave's name */
	    terminal = ptsname(fd[0]); /* returned value should not be freed */

	    if (terminal == NULL) {
		pam_syslog(pamh, LOG_ERR,
			   "Cannot get the name of the slave terminal: %m");
		close(fd[0]);
		return PAM_ABORT;
	    }

	    fd[1] = open(terminal, O_RDWR);
	    close(fd[0]);      /* process is the child -- uses line fd[1] */

	    if (fd[1] < 0) {
		pam_syslog(pamh, LOG_ERR,
			   "cannot open slave terminal: %s: %m", terminal);
		return PAM_ABORT;
	    }

	    /* initialize the child's terminal to be the way the
	       parent's was before we set it into RAW mode */

	    if ( tcsetattr(fd[1], TCSANOW, &stored_mode) < 0 ) {
		pam_syslog(pamh, LOG_ERR,
			   "cannot set slave terminal mode: %s: %m", terminal);
		close(fd[1]);
		return PAM_ABORT;
	    }
	} else {

	    /* nothing else to do for a simple stream socket */
	    close(fd[0]);

	}

	/* re-assign the stdin/out to fd[1] <- (talks to filter). */

	if ( dup2(fd[1],STDIN_FILENO) != STDIN_FILENO ||
	     dup2(fd[1],STDOUT_FILENO) != STDOUT_FILENO ||
	     dup2(fd[1],STDERR_FILENO) != STDERR_FILENO )  {
	    pam_syslog(pamh, LOG_ERR,
		       "unable to re-assign STDIN/OUT/ERR: %m");
	    close(fd[1]);
	    return PAM_ABORT;
	}

	/* now the user input is read from the parent/filter: forget fd */

	close(fd[1]);

	/* make sure that file descriptors survive 'exec's */

	if ( fcntl(STDIN_FILENO, F_SETFD, 0) ||
	     fcntl(STDOUT_FILENO,F_SETFD, 0) ||
	     fcntl(STDERR_FILENO,F_SETFD, 0) ) {
	    pam_syslog(pamh, LOG_ERR,
		       "unable to re-assign STDIN/OUT/ERR: %m");
	    return PAM_ABORT;
	}

	/* the current process is now apparently working with filtered
	   stdio/stdout/stderr --- success! */

	return PAM_SUCCESS;
    }

    if (!aterminal)
	    close(fd[1]);

    /* Clear out passwords... there is a security problem here in
     * that this process never executes pam_end.  Consequently, any
     * other sensitive data in this process is *not* explicitly
     * overwritten, before the process terminates */

    (void) pam_set_item(pamh, PAM_AUTHTOK, NULL);
    (void) pam_set_item(pamh, PAM_OLDAUTHTOK, NULL);

    /* fork a copy of process to run the actual filter executable */

    if ( (child2 = fork()) < 0 ) {

	pam_syslog(pamh, LOG_ERR, "filter fork failed: %m");
	child2 = 0;

    } else if ( child2 == 0 ) {              /* exec the child filter */

	if ( dup2(fd[0],APPIN_FILENO) != APPIN_FILENO ||
	     dup2(fd[0],APPOUT_FILENO) != APPOUT_FILENO ||
	     dup2(fd[0],APPERR_FILENO) != APPERR_FILENO )  {
	    pam_syslog(pamh, LOG_ERR,
		       "unable to re-assign APPIN/OUT/ERR: %m");
	    close(fd[0]);
	    _exit(1);
	}

	/* make sure that file descriptors survive 'exec's */

	if ( fcntl(APPIN_FILENO, F_SETFD, 0) == -1 ||
	     fcntl(APPOUT_FILENO,F_SETFD, 0) == -1 ||
	     fcntl(APPERR_FILENO,F_SETFD, 0) == -1 ) {
	    pam_syslog(pamh, LOG_ERR,
		       "unable to retain APPIN/OUT/ERR: %m");
	    close(APPIN_FILENO);
	    close(APPOUT_FILENO);
	    close(APPERR_FILENO);
	    _exit(1);
	}

	/* now the user input is read from the parent through filter */

	execle(filtername, "<pam_filter>", NULL, evp);

	/* getting to here is an error */

	pam_syslog(pamh, LOG_ERR, "filter: %s: %m", filtername);
	_exit(1);

    } else {           /* wait for either of the two children to exit */

	while (child && child2) {    /* loop if there are two children */
	    int lstatus=0;
	    int chid;

	    chid = wait(&lstatus);
	    if (chid == child) {

		if (WIFEXITED(lstatus)) {            /* exited ? */
		    status = WEXITSTATUS(lstatus);
		} else if (WIFSIGNALED(lstatus)) {   /* killed ? */
		    status = -1;
		} else
		    continue;             /* just stopped etc.. */
		child = 0;        /* the child has exited */

	    } else if (chid == child2) {
		/*
		 * if the filter has exited, let the child die
		 * naturally below
		 */
		if (WIFEXITED(lstatus) || WIFSIGNALED(lstatus))
		    child2 = 0;
	    } else {

		pam_syslog(pamh, LOG_ERR,
			   "programming error <chid=%d,lstatus=%x> "
			   "in file %s at line %d",
			   chid, lstatus, __FILE__, __LINE__);
		child = child2 = 0;
		status = -1;

	    }
	}
    }

    close(fd[0]);

    /* if there is something running, wait for it to exit */

    while (child || child2) {
	int lstatus=0;
	int chid;

	chid = wait(&lstatus);

	if (child && chid == child) {

	    if (WIFEXITED(lstatus)) {            /* exited ? */
		status = WEXITSTATUS(lstatus);
	    } else if (WIFSIGNALED(lstatus)) {   /* killed ? */
		status = -1;
	    } else
		continue;             /* just stopped etc.. */
	    child = 0;        /* the child has exited */

	} else if (child2 && chid == child2) {

	    if (WIFEXITED(lstatus) || WIFSIGNALED(lstatus))
		child2 = 0;

	} else {

	    pam_syslog(pamh, LOG_ERR,
		       "programming error <chid=%d,lstatus=%x> "
		       "in file %s at line %d",
		       chid, lstatus, __FILE__, __LINE__);
	    child = child2 = 0;
	    status = -1;

	}
    }

    if (aterminal) {
	/* reset to initial terminal mode */
	    (void) tcsetattr(STDIN_FILENO, TCSANOW, &stored_mode);
    }

    if (ctrl & FILTER_DEBUG) {
	pam_syslog(pamh, LOG_DEBUG, "parent process exited");      /* clock off */
    }

    /* quit the parent process, returning the child's exit status */

    exit(status);
    return status; /* never reached, to make gcc happy */
}

static int set_the_terminal(pam_handle_t *pamh)
{
    const void *tty;

    if (pam_get_item(pamh, PAM_TTY, &tty) != PAM_SUCCESS
	|| tty == NULL) {
	tty = ttyname(STDIN_FILENO);
	if (tty == NULL) {
	    pam_syslog(pamh, LOG_ERR, "couldn't get the tty name");
	    return PAM_ABORT;
	}
	if (pam_set_item(pamh, PAM_TTY, tty) != PAM_SUCCESS) {
	    pam_syslog(pamh, LOG_ERR, "couldn't set tty name");
	    return PAM_ABORT;
	}
    }
    return PAM_SUCCESS;
}

static int need_a_filter(pam_handle_t *pamh
			 , int flags, int argc, const char **argv
			 , const char *name, int which_run)
{
    int ctrl;
    char **evp;
    const char *filterfile;
    int retval;

    ctrl = process_args(pamh, argc, argv, name, &evp, &filterfile);
    if (ctrl == -1) {
	return PAM_AUTHINFO_UNAVAIL;
    }

    /* set the tty to the old or the new one? */

    if (!(ctrl & NON_TERM) && !(ctrl & NEW_TERM)) {
	retval = set_the_terminal(pamh);
	if (retval != PAM_SUCCESS) {
	    pam_syslog(pamh, LOG_ERR, "tried and failed to set PAM_TTY");
	}
    } else {
	retval = PAM_SUCCESS;  /* nothing to do which is always a success */
    }

    if (retval == PAM_SUCCESS && (ctrl & which_run)) {
	retval = set_filter(pamh, flags, ctrl, evp, filterfile);
    }

    if (retval == PAM_SUCCESS
	&& !(ctrl & NON_TERM) && (ctrl & NEW_TERM)) {
	retval = set_the_terminal(pamh);
	if (retval != PAM_SUCCESS) {
	    pam_syslog(pamh, LOG_ERR,
		       "tried and failed to set new terminal as PAM_TTY");
	}
    }

    free_evp(evp);

    if (ctrl & FILTER_DEBUG) {
	pam_syslog(pamh, LOG_DEBUG, "filter/%s, returning %d", name, retval);
	pam_syslog(pamh, LOG_DEBUG, "[%s]", pam_strerror(pamh, retval));
    }

    return retval;
}

/* ----------------- public functions ---------------- */

/*
 * here are the advertised access points ...
 */

/* ------------------ authentication ----------------- */

int pam_sm_authenticate(pam_handle_t *pamh,
			int flags, int argc, const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv
			 , "authenticate", FILTER_RUN1);
}

int pam_sm_setcred(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv, "setcred", FILTER_RUN2);
}

/* --------------- account management ---------------- */

int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc,
                     const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv
			 , "setcred", FILTER_RUN1|FILTER_RUN2 );
}

/* --------------- session management ---------------- */

int pam_sm_open_session(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv
			 , "open_session", FILTER_RUN1);
}

int pam_sm_close_session(pam_handle_t *pamh, int flags,
                         int argc, const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv
			 , "close_session", FILTER_RUN2);
}

/* --------- updating authentication tokens --------- */


int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
		     int argc, const char **argv)
{
    int runN;

    if (flags & PAM_PRELIM_CHECK)
	runN = FILTER_RUN1;
    else if (flags & PAM_UPDATE_AUTHTOK)
	runN = FILTER_RUN2;
    else {
	pam_syslog(pamh, LOG_ERR, "unknown flags for chauthtok (0x%X)", flags);
	return PAM_TRY_AGAIN;
    }

    return need_a_filter(pamh, flags, argc, argv, "chauthtok", runN);
}
