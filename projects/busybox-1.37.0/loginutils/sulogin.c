/* vi: set sw=4 ts=4: */
/*
 * Mini sulogin implementation for busybox
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config SULOGIN
//config:	bool "sulogin (18 kb)"
//config:	default y
//config:	select FEATURE_SYSLOG
//config:	help
//config:	sulogin is invoked when the system goes into single user
//config:	mode (this is done through an entry in inittab).

//applet:IF_SULOGIN(APPLET_NOEXEC(sulogin, sulogin, BB_DIR_SBIN, BB_SUID_DROP, sulogin))

//kbuild:lib-$(CONFIG_SULOGIN) += sulogin.o

//usage:#define sulogin_trivial_usage
//usage:       "[-t N] [TTY]"
//usage:#define sulogin_full_usage "\n\n"
//usage:       "Single user login\n"
//usage:     "\n	-p	Start a login shell"
//usage:     "\n	-t SEC	Timeout"

#include "libbb.h"
#include <syslog.h>

int sulogin_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sulogin_main(int argc UNUSED_PARAM, char **argv)
{
	int tsid;
	int timeout = 0;
	unsigned opts;
	struct passwd *pwd;
	const char *shell;

	/* Note: sulogin is not a suid app. It is meant to be run by init
	 * for single user / emergency mode. init starts it as root.
	 * Normal users (potentially malicious ones) can only run it under
	 * their UID, therefore no paranoia here is warranted:
	 * $LD_LIBRARY_PATH in env, TTY = /dev/sda
	 * are no more dangerous here than in e.g. cp applet.
	 */

	logmode = LOGMODE_BOTH;
	openlog(applet_name, 0, LOG_AUTH);

	opts = getopt32(argv, "pt:+", &timeout);
	argv += optind;

	if (argv[0]) {
		close(0);
		close(1);
		dup(xopen(argv[0], O_RDWR));
		close(2);
		dup(0);
	}

	pwd = getpwuid(0);
	if (!pwd) {
		bb_simple_error_msg_and_die("no password entry for root");
	}

	while (1) {
		int r;

		r = ask_and_check_password_extended(pwd, timeout,
			"Give root password for maintenance\n"
			"(or type Ctrl-D to continue): "
		);
		if (r < 0) {
			/* ^D, ^C, timeout, or read error */
			/* util-linux 2.36.1 compat: no message */
			/*bb_simple_info_msg("normal startup");*/
			return 0;
		}
		if (r > 0) {
			break;
		}
		pause_after_failed_login();
		bb_simple_info_msg("Login incorrect");
	}

	/* util-linux 2.36.1 compat: no message */
	/*bb_simple_info_msg("starting shell for system maintenance");*/

	IF_SELINUX(renew_current_security_context());

	shell = getenv("SUSHELL");
	if (!shell)
		shell = getenv("sushell");
	if (!shell)
		shell = pwd->pw_shell;

	/* util-linux 2.36.1 compat: cd to root's HOME, set a few envvars */
	setup_environment(shell, 0
		+ SETUP_ENV_CHANGEENV_LOGNAME
		+ SETUP_ENV_CHDIR
		, pwd);
	// no SETUP_ENV_CLEARENV
	// SETUP_ENV_CHANGEENV_LOGNAME - set HOME, SHELL, USER,and LOGNAME
	// SETUP_ENV_CHDIR - cd to $HOME

	/* util-linux 2.36.1 compat: steal ctty if we don't have it yet
	 * (yes, util-linux uses force=1)  */
	tsid = tcgetsid(STDIN_FILENO);
	if (tsid < 0 || getpid() != tsid) {
		if (ioctl(STDIN_FILENO, TIOCSCTTY, /*force:*/ (long)1) != 0) {
//			bb_perror_msg("TIOCSCTTY1 tsid:%d", tsid);
			if (setsid() > 0) {
//				bb_error_msg("done setsid()");
				/* If it still does not work, ignore */
				if (ioctl(STDIN_FILENO, TIOCSCTTY, /*force:*/ (long)1) != 0) {
//					bb_perror_msg("TIOCSCTTY2 tsid:%d", tsid);
				}
			}
		}
	}

	/*
	 * Note: login does this (should we do it too?):
	 */
	/*signal(SIGINT, SIG_DFL);*/

	/* Exec shell with no additional parameters. Never returns. */
	exec_shell(shell, /* -p? then shell is login:*/(opts & 1), NULL);
}
