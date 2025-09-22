/*
 * sulogin
 *
 * This program gives Linux machines a reasonable secure way to boot single
 * user. It forces the user to supply the root password before a shell is
 * started. If there is a shadow password file and the encrypted root password
 * is "x" the shadow password will be used.
 *
 * Copyright (C) 1998-2003 Miquel van Smoorenburg.
 * Copyright (C) 2012 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2012 Werner Fink <werner@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <shadow.h>
#include <termios.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ioctl.h>
#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif

#ifdef HAVE_LIBSELINUX
# include <selinux/selinux.h>
# include <selinux/get_context_list.h>
#endif

#ifdef __linux__
# include <sys/kd.h>
# include <sys/param.h>
# include <linux/serial.h>
#endif

#include "c.h"
#include "closestream.h"
#include "env.h"
#include "nls.h"
#include "pathnames.h"
#ifdef USE_PLYMOUTH_SUPPORT
# include "plymouth-ctrl.h"
#endif
#include "strutils.h"
#include "ttyutils.h"
#include "sulogin-consoles.h"
#define CONMAX		16

static unsigned int timeout;
static int profile;
static volatile uint32_t openfd;		/* Remember higher file descriptors */

static struct sigaction saved_sigint;
static struct sigaction saved_sigtstp;
static struct sigaction saved_sigquit;
static struct sigaction saved_sighup;
static struct sigaction saved_sigchld;

static volatile sig_atomic_t alarm_rised;
static volatile sig_atomic_t sigchild;

#define SULOGIN_PASSWORD_BUFSIZ	128

#ifndef IUCLC
# define IUCLC		0
#endif

#ifndef WEXITED
# warning "WEXITED is missing, sulogin may not work as expected"
# define WEXITED 0
#endif

static int locked_account_password(const char * const passwd)
{
	if (passwd && (*passwd == '*' || *passwd == '!'))
		return 1;
	return 0;
}

#ifdef HAVE_LIBSELINUX
/*
 * Cached check whether SELinux is enabled.
 */
static int is_selinux_enabled_cached(void)
{
	static int cache = -1;

	if (cache == -1)
		cache = is_selinux_enabled();

	return cache;
}

/* Computed SELinux login context. */
static char *login_context;

/*
 * Compute SELinux login context.
 */
static void compute_login_context(void)
{
	char *seuser = NULL;
	char *level = NULL;

	if (is_selinux_enabled_cached() == 0)
		goto cleanup;

	if (getseuserbyname("root", &seuser, &level) == -1) {
		warnx(_("failed to compute seuser"));
		goto cleanup;
	}

	if (get_default_context_with_level(seuser, level, NULL, &login_context) == -1) {
		warnx(_("failed to compute default context"));
		goto cleanup;
	}

cleanup:
	free(seuser);
	free(level);
}

/*
 * Compute SELinux terminal context.
 */
static void tcinit_selinux(struct console *con)
{
	security_class_t tclass;

	if (!login_context)
		return;

	if (fgetfilecon(con->fd, &con->reset_tty_context) == -1) {
		warn(_("failed to get context of terminal %s"), con->tty);
		return;
	}

	tclass = string_to_security_class("chr_file");
	if (tclass == 0) {
		warnx(_("security class chr_file not available"));
		freecon(con->reset_tty_context);
		con->reset_tty_context = NULL;
		return;
	}

	if (security_compute_relabel(login_context, con->reset_tty_context, tclass, &con->user_tty_context) == -1) {
		warnx(_("failed to compute relabel context of terminal"));
		freecon(con->reset_tty_context);
		con->reset_tty_context = NULL;
		return;
	}
}
#endif

/*
 * Fix the tty modes and set reasonable defaults.
 */
static void tcinit(struct console *con)
{
	int flags = 0, mode = 0;
	struct termios *tio = &con->tio;
	const int fd = con->fd;
#if defined(TIOCGSERIAL)
	struct serial_struct serinfo = { .flags = 0 };
#endif
#ifdef USE_PLYMOUTH_SUPPORT
	struct termios lock;
	int i = (plymouth_command(MAGIC_PING)) ? PLYMOUTH_TERMIOS_FLAGS_DELAY : 0;
	if (i)
		plymouth_command(MAGIC_QUIT);
	while (i-- > 0) {
		/*
		 * With plymouth the termios flags become changed after this
		 * function had changed the termios.
		 */
		memset(&lock, 0, sizeof(struct termios));
		if (ioctl(fd, TIOCGLCKTRMIOS, &lock) < 0)
			break;
		if (!lock.c_iflag && !lock.c_oflag && !lock.c_cflag && !lock.c_lflag)
			break;
		sleep(1);
	}
	memset(&lock, 0, sizeof(struct termios));
	ioctl(fd, TIOCSLCKTRMIOS, &lock);
	errno = 0;
#endif

#ifdef HAVE_LIBSELINUX
	tcinit_selinux(con);
#endif

#ifdef TIOCGSERIAL
	if (ioctl(fd, TIOCGSERIAL,  &serinfo) >= 0)
		con->flags |= CON_SERIAL;
	errno = 0;
#endif

#ifdef KDGKBMODE
	if (!(con->flags & CON_SERIAL)
	    && ioctl(fd, KDGKBMODE, &mode) < 0)
		con->flags |= CON_SERIAL;
	errno = 0;
#endif
	if (tcgetattr(fd, tio) < 0) {
		int saveno = errno;
#if defined(KDGKBMODE) || defined(TIOCGSERIAL)
		if (con->flags & CON_SERIAL) {			/* Try to recover this */

# if defined(TIOCGSERIAL)
			serinfo.flags |= ASYNC_SKIP_TEST;	/* Skip test of UART */

			if (ioctl(fd, TIOCSSERIAL, &serinfo) < 0)
				goto tcgeterr;
			if (ioctl(fd, TIOCSERCONFIG) < 0)	/* Try to autoconfigure */
				goto tcgeterr;
			if (ioctl(fd, TIOCGSERIAL,  &serinfo) < 0)
				goto tcgeterr;			/* Ouch */
# endif
			if (tcgetattr(fd, tio) < 0)		/* Retry to get tty attributes */
				saveno = errno;
		}
# if defined(TIOCGSERIAL)
	tcgeterr:
# endif
		if (saveno)
#endif
		{
			FILE *fcerr = fdopen(fd, "w");
			if (fcerr) {
				fprintf(fcerr, _("tcgetattr failed"));
				fclose(fcerr);
			}
			warn(_("tcgetattr failed"));

			con->flags &= ~CON_SERIAL;
			if (saveno != EIO)
				con->flags |= CON_NOTTY;
			else
				con->flags |= CON_EIO;

			errno = 0;
			return;
		}
	}

	/* Handle lines other than virtual consoles here */
#if defined(KDGKBMODE) || defined(TIOCGSERIAL)
	if (con->flags & CON_SERIAL)
#endif
	{
		speed_t ispeed, ospeed;
		struct winsize ws;
		errno = 0;

		/* Flush input and output queues on modem lines */
		tcflush(fd, TCIOFLUSH);

		ispeed = cfgetispeed(tio);
		ospeed = cfgetospeed(tio);

		if (!ispeed) ispeed = TTYDEF_SPEED;
		if (!ospeed) ospeed = TTYDEF_SPEED;

		tio->c_cflag = CREAD | CS8 | HUPCL | (tio->c_cflag & CLOCAL);
		tio->c_iflag = 0;
		tio->c_lflag = 0;
		tio->c_oflag &= OPOST | ONLCR;

		cfsetispeed(tio, ispeed);
		cfsetospeed(tio, ospeed);

#ifdef HAVE_STRUCT_TERMIOS_C_LINE
		tio->c_line         = 0;
#endif
		tio->c_cc[VTIME]    = 0;
		tio->c_cc[VMIN]     = 1;

		if (ioctl(fd, TIOCGWINSZ, &ws) == 0) {
			int update = 0;

			if (ws.ws_row == 0) {
				ws.ws_row = 24;
				update++;
			}
			if (ws.ws_col == 0) {
				ws.ws_col = 80;
				update++;
			}
			if (update)
				ignore_result( ioctl(fd, TIOCSWINSZ, &ws) );
		}

		setlocale(LC_CTYPE, "POSIX");
		goto setattr;
	}
#if defined(IUTF8) && defined(KDGKBMODE)
	/* Handle mode of current keyboard setup, e.g. for UTF-8 */
	switch(mode) {
	case K_UNICODE:
		setlocale(LC_CTYPE, "C.UTF-8");
		flags |= UL_TTY_UTF8;
		break;
	case K_RAW:
	case K_MEDIUMRAW:
	case K_XLATE:
	default:
		setlocale(LC_CTYPE, "POSIX");
		break;
	}
#else
	setlocale(LC_CTYPE, "POSIX");
#endif
	reset_virtual_console(tio, flags);
setattr:
	if (tcsetattr(fd, TCSANOW, tio))
		warn(_("tcsetattr failed"));

	/* Enable blocking mode for read and write */
	if ((flags = fcntl(fd, F_GETFL, 0)) != -1)
		ignore_result( fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) );
}

/*
 * Finalize the tty modes on modem lines.
 */
static void tcfinal(struct console *con)
{
	struct termios *tio = &con->tio;
	const int fd = con->fd;
	char *term, *ttyname = NULL;

	if (con->tty)
		ttyname = strncmp(con->tty, "/dev/", 5) == 0 ?
					con->tty + 5 : con->tty;

	term = get_terminal_default_type(ttyname, con->flags & CON_SERIAL);
	if (term) {
		xsetenv("TERM", term, 0);
		free(term);
	}

	if (!(con->flags & CON_SERIAL) || (con->flags & CON_NOTTY))
		return;

	tio->c_iflag |= (IXON | IXOFF);
	tio->c_lflag |= (ICANON | ISIG | ECHO|ECHOE|ECHOK|ECHOKE);
	tio->c_oflag |= OPOST;

	tio->c_cc[VINTR]    = CINTR;
	tio->c_cc[VQUIT]    = CQUIT;
	tio->c_cc[VERASE]   = con->cp.erase;
	tio->c_cc[VKILL]    = con->cp.kill;
	tio->c_cc[VEOF]     = CEOF;
#ifdef VSWTC
	tio->c_cc[VSWTC]    = _POSIX_VDISABLE;
#elif defined(VSWTCH)
	tio->c_cc[VSWTCH]   = _POSIX_VDISABLE;
#endif
	tio->c_cc[VSTART]   = CSTART;
	tio->c_cc[VSTOP]    = CSTOP;
	tio->c_cc[VSUSP]    = CSUSP;
	tio->c_cc[VEOL]     = _POSIX_VDISABLE;

	if (con->cp.eol == CR) {
		tio->c_iflag |= ICRNL;
		tio->c_iflag &= ~(INLCR|IGNCR);
		tio->c_oflag |= ONLCR;
		tio->c_oflag &= ~(OCRNL|ONLRET);
	}

	switch (con->cp.parity) {
	default:
	case 0:
		tio->c_cflag &= ~(PARODD | PARENB);
		tio->c_iflag &= ~(INPCK | ISTRIP);
		break;
	case 1:				/* odd parity */
		tio->c_cflag |= PARODD;
		/* fallthrough */
	case 2:				/* even parity */
		tio->c_cflag |= PARENB;
		tio->c_iflag |= (INPCK | ISTRIP);
		/* fallthrough */
	case (1 | 2):			/* no parity bit */
		tio->c_cflag &= ~CSIZE;
		tio->c_cflag |= CS7;
		break;
	}

	/* Set line attributes */
	tcsetattr(fd, TCSANOW, tio);
}

/*
 * Called at timeout.
 */
static void alrm_handler(int sig __attribute__((unused)))
{
	/* Timeout expired */
	alarm_rised = 1;
}

static void chld_handler(int sig __attribute__((unused)))
{
	sigchild = 1;
}

static void mask_signal(int signal, void (*handler)(int),
		struct sigaction *origaction)
{
	struct sigaction newaction;

	newaction.sa_handler = handler;
	sigemptyset(&newaction.sa_mask);
	newaction.sa_flags = 0;

	sigaction(signal, &newaction, origaction);
}

static void unmask_signal(int signal, struct sigaction *sa)
{
	sigaction(signal, sa, NULL);
}

/*
 * See if an encrypted password is valid. The encrypted password is checked for
 * traditional-style DES and FreeBSD-style MD5 encryption.
 */
static int valid(const char *pass)
{
	const char *s;
	char id[5];
	size_t len;
	off_t off;

	if (pass[0] == 0)
		return 1;
	if (pass[0] != '$')
		goto check_des;

	/*
	 * up to 4 bytes for the signature e.g. $1$
	 */
	for (s = pass+1; *s && *s != '$'; s++);

	if (*s++ != '$')
		return 0;

	if ((off = (off_t)(s-pass)) > 4 || off < 3)
		return 0;

	memset(id, '\0', sizeof(id));
	strncpy(id, pass, off);

	/*
	 * up to 16 bytes for the salt
	 */
	for (; *s && *s != '$'; s++);

	if (*s++ != '$')
		return 0;

	if ((off_t)(s-pass) > 16)
		return 0;

	len = strlen(s);

	/*
	 * the MD5 hash (128 bits or 16 bytes) encoded in base64 = 22 bytes
	 */
	if ((strcmp(id, "$1$") == 0) && (len < 22 || len > 24))
		return 0;

	/*
	 * the SHA-256 hash 43 bytes
	 */
	if ((strcmp(id, "$5$") == 0) && (len < 42 || len > 44))
		return 0;

	/*
	 * the SHA-512 hash 86 bytes
	 */
	if ((strcmp(id, "$6$") == 0) && (len < 85 || len > 87))
		return 0;

	/*
	 * e.g. Blowfish hash
	 */
	return 1;
check_des:
	if (strlen(pass) != 13)
		return 0;

	for (s = pass; *s; s++) {
		if ((*s < '0' || *s > '9') &&
		    (*s < 'a' || *s > 'z') &&
		    (*s < 'A' || *s > 'Z') &&
		    *s != '.' && *s != '/')
			return 0;
	}
	return 1;
}

/*
 * Set a variable if the value is not NULL.
 */
static inline void set(char **var, char *val)
{
	if (val)
		*var = val;
}

/*
 * Get the root password entry.
 */
static struct passwd *getrootpwent(int try_manually)
{
	static struct passwd pwd;
	struct passwd *pw;
	struct spwd *spw;
	FILE *fp;
	static char line[2 * BUFSIZ];
	static char sline[2 * BUFSIZ];
	char *p;

	/*
	 * First, we try to get the password the standard way using normal
	 * library calls.
	 */
	if ((pw = getpwnam("root")) &&
	    !strcmp(pw->pw_passwd, "x") &&
	    (spw = getspnam("root")))
		pw->pw_passwd = spw->sp_pwdp;

	if (pw || !try_manually)
		return pw;

	/*
	 * If we come here, we could not retrieve the root password through
	 * library calls and we try to read the password and shadow files
	 * manually.
	 */
	pwd.pw_name = "root";
	pwd.pw_passwd = "";
	pwd.pw_gecos = "Super User";
	pwd.pw_dir = "/";
	pwd.pw_shell = "";
	pwd.pw_uid = 0;
	pwd.pw_gid = 0;

	if ((fp = fopen(_PATH_PASSWD, "r")) == NULL) {
		warn(_("cannot open %s"), _PATH_PASSWD);
		return &pwd;
	}

	/*
	 * Find root in the password file.
	 */
	while ((p = fgets(line, sizeof(line), fp)) != NULL) {
		if (strncmp(line, "root:", 5) != 0)
			continue;
		p += 5;
		set(&pwd.pw_passwd, strsep(&p, ":"));
		strsep(&p, ":");
		strsep(&p, ":");
		set(&pwd.pw_gecos, strsep(&p, ":"));
		set(&pwd.pw_dir, strsep(&p, ":"));
		set(&pwd.pw_shell, strsep(&p, "\n"));
		p = line;
		break;
	}
	fclose(fp);

	/*
	 * If the encrypted password is valid or not found, return.
	 */
	if (p == NULL) {
		warnx(_("%s: no entry for root\n"), _PATH_PASSWD);
		return &pwd;
	}
	if (valid(pwd.pw_passwd))
		return &pwd;

	/*
	 * The password is invalid. If there is a shadow password, try it.
	 */
	*pwd.pw_passwd = '\0';
	if ((fp = fopen(_PATH_SHADOW_PASSWD, "r")) == NULL) {
		warn(_("cannot open %s"), _PATH_PASSWD);
		return &pwd;
	}
	while ((p = fgets(sline, sizeof(sline), fp)) != NULL) {
		if (strncmp(sline, "root:", 5) != 0)
			continue;
		p += 5;
		set(&pwd.pw_passwd, strsep(&p, ":"));
		break;
	}
	fclose(fp);

	/*
	 * If the password is still invalid, NULL it, and return.
	 */
	if (p == NULL) {
		warnx(_("%s: no entry for root"), _PATH_SHADOW_PASSWD);
		*pwd.pw_passwd = '\0';
	}
	/* locked account passwords are valid too */
	if (!locked_account_password(pwd.pw_passwd) && !valid(pwd.pw_passwd)) {
		warnx(_("%s: root password garbled"), _PATH_SHADOW_PASSWD);
		*pwd.pw_passwd = '\0';
	}
	return &pwd;
}

/*
 * Ask by prompt for the password.
 */
static void doprompt(const char *crypted, struct console *con, int deny)
{
	struct termios tty;

	if (con->flags & CON_SERIAL) {
		tty = con->tio;
		/*
		 * For prompting: map NL in output to CR-NL
		 * otherwise we may see stairs in the output.
		 */
		tty.c_oflag |= (ONLCR | OPOST);
		tcsetattr(con->fd, TCSADRAIN, &tty);
	}
	if (!con->file) {
		con->file = fdopen(con->fd, "r+");
		if (!con->file)
			goto err;
	}

	if (deny)
		fprintf(con->file, _("\nCannot open access to console, the root account is locked.\n"
				     "See sulogin(8) man page for more details.\n\n"
				     "Press Enter to continue.\n"));
	else {
#if defined(USE_ONELINE)
		if (crypted[0] && !locked_account_password(crypted))
			fprintf(con->file, _("Give root password for login: "));
		else
			fprintf(con->file, _("Press Enter for login: "));
#else
		if (crypted[0] && !locked_account_password(crypted))
			fprintf(con->file, _("Give root password for maintenance\n"));
		else
			fprintf(con->file, _("Press Enter for maintenance\n"));
		fprintf(con->file, _("(or press Control-D to continue): "));
#endif
	}
	fflush(con->file);
err:
	if (con->flags & CON_SERIAL)
		tcsetattr(con->fd, TCSADRAIN, &con->tio);
}

/*
 * Make sure to have an own session and controlling terminal
 */
static void setup(struct console *con)
{
	int fd = con->fd;
	const pid_t pid = getpid(), pgrp = getpgid(0), ppgrp = getpgid(getppid());
	pid_t ttypgrp;

	if (con->flags & CON_NOTTY)
		goto notty;
	if (con->flags & CON_EIO)
		return;

	ttypgrp = tcgetpgrp(fd);

	/*
	 * Only go through this trouble if the new
	 * tty doesn't fall in this process group.
	 */
	if (pgrp != ttypgrp && ppgrp != ttypgrp) {
		if (pid != getsid(0)) {
			if (pid == getpgid(0))
				setpgid(0, getpgid(getppid()));
			setsid();
		}

		mask_signal(SIGHUP, SIG_IGN, &saved_sighup);
		if (ttypgrp > 0)
			ioctl(STDIN_FILENO, TIOCNOTTY, (char *)1);
		unmask_signal(SIGHUP, &saved_sighup);
		if (fd > STDIN_FILENO)  close(STDIN_FILENO);
		if (fd > STDOUT_FILENO) close(STDOUT_FILENO);
		if (fd > STDERR_FILENO) close(STDERR_FILENO);

		ioctl(fd, TIOCSCTTY, (char *)1);
		tcsetpgrp(fd, ppgrp);
	}
notty:
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	con->fd = STDIN_FILENO;

	for (fd = STDERR_FILENO+1; fd < 32; fd++) {
		if (openfd & (1<<fd)) {
			close(fd);
			openfd &= ~(1<<fd);
		}
	}
}

/*
 * Ask for the password. Note that there is no default timeout as we normally
 * skip this during boot.
 */
static char *getpasswd(struct console *con)
{
	struct sigaction sa;
	struct termios tty;
	static char pass[SULOGIN_PASSWORD_BUFSIZ], *ptr;
	struct chardata *cp;
	char *ret = NULL;
	unsigned char tc;
	char c, ascval;
	int eightbit;
	const int fd = con->fd;

	if (con->flags & CON_EIO)
		goto out;

	cp = &con->cp;
	tty = con->tio;
	tc = 0;
	ret = pass;

	tty.c_iflag &= ~(IUCLC|IXON|IXOFF|IXANY);
	tty.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|TOSTOP|ISIG);

	if ((con->flags & CON_NOTTY) == 0)
		tc = (tcsetattr(fd, TCSAFLUSH, &tty) == 0);

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = alrm_handler;
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, NULL);

	if (timeout)
		alarm(timeout);

	ptr = &pass[0];
	cp->eol = *ptr = '\0';

	eightbit = ((con->flags & CON_SERIAL) == 0 || (tty.c_cflag & (PARODD|PARENB)) == 0);

	while (cp->eol == '\0') {
		errno = 0;

		if (read(fd, &c, 1) < 1) {
			if (errno == EINTR || errno == EAGAIN) {
				if (alarm_rised) {
					ret = NULL;
					goto quit;
				}
				xusleep(250000);
				continue;
			}
			ret = NULL;

			switch (errno) {
			case EIO:
				con->flags |= CON_EIO;
				/* fallthrough */
			default:
				warn(_("cannot read %s"), con->tty);
				break;
			case 0:
				break;
			}
			goto quit;
		}

		if (eightbit)
			ascval = c;
		else if (c != (ascval = (c & 0177))) {
			uint32_t bits, mask;
			for (bits = 1, mask = 1; mask & 0177; mask <<= 1) {
				if (mask & ascval)
					bits++;
			}
			cp->parity |= ((bits & 1) ? 1 : 2);
		}

		switch (ascval) {
		case 0:
			*ptr = '\0';
			goto quit;
		case CR:
		case NL:
			*ptr = '\0';
			cp->eol = ascval;
			break;
		case BS:
		case CERASE:
			cp->erase = ascval;
			if (ptr > &pass[0])
				ptr--;
			break;
		case CKILL:
			cp->kill = ascval;
			while (ptr > &pass[0])
				ptr--;
			break;
		case CEOF:
			ret = NULL;
			goto quit;
		default:
			if ((size_t)(ptr - &pass[0]) >= (sizeof(pass) -1 )) {
				 fprintf(stderr, "sulogin: input overrun at %s\n\r", con->tty);
				 ret = NULL;
				 goto quit;
			}
			*ptr++ = ascval;
			break;
		}
	}
quit:
	alarm(0);
	if (tc)
		tcsetattr(fd, TCSAFLUSH, &con->tio);
	tcfinal(con);
	printf("\r\n");
out:
#ifdef HAVE_EXPLICIT_BZERO
	if (ret == NULL)
		explicit_bzero(pass, sizeof(pass));
#endif
	return ret;
}

/*
 * Password was OK, execute a shell.
 */
static void sushell(struct passwd *pwd, struct console *con)
{
	char shell[PATH_MAX];
	char home[PATH_MAX];
	char const *p;
	char const *su_shell;

	/*
	 * Set directory and shell.
	 */
	if (chdir(pwd->pw_dir) != 0) {
		warn(_("%s: change directory failed"), pwd->pw_dir);
		printf(_("Logging in with home = \"/\".\n"));

		if (chdir("/") != 0)
			warn(_("change directory to system root failed"));
	}

	if ((p = getenv("SUSHELL")) != NULL)
		su_shell = p;
	else if ((p = getenv("sushell")) != NULL)
		su_shell = p;
	else {
		if (pwd->pw_shell[0])
			su_shell = pwd->pw_shell;
		else
			su_shell = "/bin/sh";
	}
	if ((p = strrchr(su_shell, '/')) == NULL)
		p = su_shell;
	else
		p++;

	snprintf(shell, sizeof(shell), profile ? "-%s" : "%s", p);

	/*
	 * Set some important environment variables.
	 */
	if (getcwd(home, sizeof(home)) == NULL)
		strcpy(home, "/");

	xsetenv("HOME", home, 1);
	xsetenv("LOGNAME", "root", 1);
	xsetenv("USER", "root", 1);
	if (!profile)
		xsetenv("SHLVL","0",1);

	/*
	 * Try to execute a shell.
	 */
	xsetenv("SHELL", su_shell, 1);
	unmask_signal(SIGINT, &saved_sigint);
	unmask_signal(SIGTSTP, &saved_sigtstp);
	unmask_signal(SIGQUIT, &saved_sigquit);
	mask_signal(SIGHUP, SIG_DFL, NULL);

#ifdef HAVE_LIBSELINUX
	if (is_selinux_enabled_cached() == 1) {
		if (con->user_tty_context) {
			if (fsetfilecon(con->fd, con->user_tty_context) == -1)
				warn(_("failed to set context to %s for terminal %s"), con->user_tty_context, con->tty);
		}

		if (login_context) {
			if (setexeccon(login_context) == -1)
				warn(_("failed to set exec context to %s"), login_context);
		}
	}
#else
	(void)con;
#endif

	execl(su_shell, shell, (char *)NULL);
	warn(_("failed to execute %s"), su_shell);

	xsetenv("SHELL", "/bin/sh", 1);
	execl("/bin/sh", profile ? "-sh" : "sh", (char *)NULL);
	warn(_("failed to execute %s"), "/bin/sh");
}

#ifdef HAVE_LIBSELINUX
static void tcreset_selinux(struct list_head *consoles) {
	struct list_head *ptr;
	struct console *con;

	if (is_selinux_enabled_cached() == 0)
		return;

	list_for_each(ptr, consoles) {
		con = list_entry(ptr, struct console, entry);

		if (con->fd < 0)
			continue;
		if (!con->reset_tty_context)
			continue;
		if (fsetfilecon(con->fd, con->reset_tty_context) == -1)
			warn(_("failed to reset context to %s for terminal %s"), con->reset_tty_context, con->tty);

		freecon(con->reset_tty_context);
		con->reset_tty_context = NULL;
	}
}
#endif

static void usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(
		" %s [options] [tty device]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Single-user login.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -p, --login-shell        start a login shell\n"
		" -t, --timeout <seconds>  max time to wait for a password (default: no limit)\n"
		" -e, --force              examine password files directly if getpwnam(3) fails\n"),
		out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(26));
	fprintf(out, USAGE_MAN_TAIL("sulogin(8)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	struct list_head *ptr, consoles;
	struct console *con;
	char *tty = NULL;
	struct passwd *pwd;
	const struct timespec sigwait = { .tv_sec = 0, .tv_nsec = 50000000 };
	siginfo_t status = { 0 };
	sigset_t set;
	int c, reconnect = 0;
	int opt_e = 0;
	int wait = 0;
	pid_t pid;

	static const struct option longopts[] = {
		{ "login-shell",  no_argument,       NULL, 'p' },
		{ "timeout",      required_argument, NULL, 't' },
		{ "force",        no_argument,       NULL, 'e' },
		{ "help",         no_argument,       NULL, 'h' },
		{ "version",      no_argument,       NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	INIT_LIST_HEAD(&consoles);

	/*
	 * If we are init we need to set up an own session.
	 */
	if ((pid = getpid()) == 1) {
		setsid();
		ignore_result( ioctl(STDIN_FILENO, TIOCSCTTY, (char *) 1) );
	}

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	/*
	 * See if we have a timeout flag.
	 */
	while ((c = getopt_long(argc, argv, "ehpt:V", longopts, NULL)) != -1) {
		switch(c) {
		case 't':
			timeout = strtou32_or_err(optarg, _("invalid timeout argument"));
			break;
		case 'p':
			profile = 1;
			break;
		case 'e':
			opt_e = 1;
			break;
		case 'V':
		{
			static const char *features[] = {
#ifdef USE_SULOGIN_EMERGENCY_MOUNT
				"emergency-mount",
#endif
				NULL
			};
			print_version_with_features(EXIT_SUCCESS, features);
		}
		case 'h':
			usage();
		default:
			/* Do not exit! getopt prints a warning. */
			break;
		}
	}

	if (geteuid() != 0)
		errx(EXIT_FAILURE, _("only superuser can run this program"));

	mask_signal(SIGQUIT, SIG_IGN, &saved_sigquit);
	mask_signal(SIGTSTP, SIG_IGN, &saved_sigtstp);
	mask_signal(SIGINT,  SIG_IGN, &saved_sigint);
	mask_signal(SIGHUP,  SIG_IGN, &saved_sighup);


	emergency_do_mounts();
	atexit( emergency_do_umounts );

	/*
	 * See if we need to open an other tty device.
	 */
	if (optind < argc)
		tty = argv[optind];

	if (!tty || *tty == '\0')
		tty = getenv("CONSOLE");

	/*
	 * Detect possible consoles, use stdin as fallback.
	 * If an optional tty is given, reconnect it to stdin.
	 */
	reconnect = detect_consoles(tty, STDIN_FILENO, &consoles);

	/*
	 * If previous stdin was not the specified tty and therefore reconnected
	 * to the specified tty also reconnect stdout and stderr.
	 */
	if (reconnect) {
		if (isatty(STDOUT_FILENO) == 0)
			dup2(STDOUT_FILENO, STDIN_FILENO);
		if (isatty(STDERR_FILENO) == 0)
			dup2(STDOUT_FILENO, STDERR_FILENO);
	}

	/*
	 * Should not happen
	 */
	if (list_empty(&consoles)) {
		if (!errno)
			errno = ENOENT;
		err(EXIT_FAILURE, _("cannot open console"));
	}

	/*
	 * Get the root password.
	 */
	if ((pwd = getrootpwent(opt_e)) == NULL) {
		warnx(_("cannot open password database"));
		sleep(2);
		return EXIT_FAILURE;
	}

#ifdef HAVE_LIBSELINUX
	compute_login_context();
#endif

	/*
	 * Ask for the password on the consoles.
	 */
	list_for_each(ptr, &consoles) {
		con = list_entry(ptr, struct console, entry);
		if (con->id >= CONMAX)
			break;
		if (con->fd >= 0) {
			openfd |= (1 << con->fd);
			tcinit(con);
			continue;
		}
		if ((con->fd = open(con->tty, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
			continue;
		openfd |= (1 << con->fd);
		tcinit(con);
	}
	ptr = (&consoles)->next;

#ifdef HAVE_LIBSELINUX
	/*
	 * Always fork with SELinux enabled, so the parent can restore the
	 * terminal context afterwards.
	 */
	if (is_selinux_enabled_cached() == 0)
#endif
	{
		if (ptr->next == &consoles) {
			con = list_entry(ptr, struct console, entry);
			goto nofork;
		}
	}


	mask_signal(SIGCHLD, chld_handler, &saved_sigchld);
	do {
		con = list_entry(ptr, struct console, entry);
		if (con->id >= CONMAX)
			break;
		if (con->flags & CON_EIO)
			goto next;

		switch ((con->pid = fork())) {
		case 0:
			mask_signal(SIGCHLD, SIG_DFL, NULL);
			dup2(con->fd, STDERR_FILENO);
		nofork:
			setup(con);
			while (1) {
				const char *passwd = pwd->pw_passwd;
				char *answer;
				int doshell = 0;
				int deny = !opt_e && locked_account_password(pwd->pw_passwd);

				doprompt(passwd, con, deny);

				if ((answer = getpasswd(con)) == NULL)
					break;
				if (deny) {
#ifdef HAVE_EXPLICIT_BZERO
					explicit_bzero(answer, SULOGIN_PASSWORD_BUFSIZ);
#endif
					exit(EXIT_FAILURE);
				}

				/* no password or locked account */
				if (!passwd[0] || locked_account_password(passwd))
					doshell++;
				else {
					const char *cryptbuf;
					cryptbuf = crypt(answer, passwd);
					if (cryptbuf == NULL)
						warn(_("crypt failed"));
					else if (strcmp(cryptbuf, pwd->pw_passwd) == 0)
						doshell++;
				}
#ifdef HAVE_EXPLICIT_BZERO
				explicit_bzero(answer, SULOGIN_PASSWORD_BUFSIZ);
#endif
				if (doshell) {
					/* sushell() unmask signals */
					sushell(pwd, con);

					mask_signal(SIGQUIT, SIG_IGN, &saved_sigquit);
					mask_signal(SIGTSTP, SIG_IGN, &saved_sigtstp);
					mask_signal(SIGINT,  SIG_IGN, &saved_sigint);

					fprintf(stderr, _("cannot execute su shell\n\n"));
					break;
				}
				fprintf(stderr, _("Login incorrect\n\n"));
			}
			if (alarm_rised) {
				tcfinal(con);
				warnx(_("Timed out\n\n"));
			}
			/*
			 * User pressed Control-D.
			 */
			exit(0);
		case -1:
			warn(_("fork failed"));
			/* fallthrough */
		default:
			break;
		}
	next:
		ptr = ptr->next;

	} while (ptr != &consoles);

	do {
		int ret;

		status.si_pid = 0;
		ret = waitid(P_ALL, 0, &status, WEXITED);

		if (ret == 0)
			break;
		if (ret < 0) {
			if (errno == ECHILD)
				break;
			if (errno == EINTR)
				continue;
		}

		errx(EXIT_FAILURE, _("cannot wait on su shell\n\n"));

	} while (1);

	list_for_each(ptr, &consoles) {
		con = list_entry(ptr, struct console, entry);

		if (con->fd < 0)
			continue;
		if (con->pid < 0)
			continue;
		if (con->pid == status.si_pid)
			con->pid = -1;
		else {
			kill(con->pid, SIGTERM);
			wait++;
		}
	}

	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);

	do {
		int signum, ret;

		if (!wait)
			break;

		status.si_pid = 0;
		ret = waitid(P_ALL, 0, &status, WEXITED|WNOHANG);

		if (ret < 0) {
			if (errno == ECHILD)
				break;
			if (errno == EINTR)
				continue;
		}

		if (!ret && status.si_pid > 0) {
			list_for_each(ptr, &consoles) {
				con = list_entry(ptr, struct console, entry);

				if (con->fd < 0)
					continue;
				if (con->pid < 0)
					continue;
				if (con->pid == status.si_pid) {
					con->pid = -1;
					wait--;
				}
			}
			continue;
		}

		signum = sigtimedwait(&set, NULL, &sigwait);
		if (signum != SIGCHLD && signum < 0 && errno == EAGAIN)
			break;

	} while (1);

	mask_signal(SIGCHLD, SIG_DFL, NULL);

#ifdef HAVE_LIBSELINUX
	tcreset_selinux(&consoles);
#endif

	return EXIT_SUCCESS;
}
