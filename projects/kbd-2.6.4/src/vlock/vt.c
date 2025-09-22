
/*

  VT code and signal handling for vlock, the VT locking program for linux.

  Copyright (C) 1994-1998  Michael K. Johnson <johnsonm@redhat.com>
  Copyright (C) 2002, 2004, 2005  Dmitry V. Levin <ldv@altlinux.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/vt.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include "vlock.h"
#include "libcommon.h"

/* Saved VT mode. */
struct vt_mode ovtm;

/* VT descriptor. */
static int vfd = -1;

/* Copy of the VT mode when the program was started. */
int is_vt;

/*
 * This is called by a signal whenever a user tries to change
 * the VC (with a ALT-Fn key or via VT_ACTIVATE).
 */
static void
release_vt(__attribute__((unused)) int signo)
{
	/*
	 * Kernel is not allowed to switch.
	 * Return code is silently ignored.
	 */
	ioctl(vfd, VT_RELDISP, 0);
}

/* This is called whenever a user switches to that VC. */
static void
acquire_vt(__attribute__((unused)) int signo)
{
	/*
	 * This call is not currently required under Linux,
	 * but it won't hurt, either.
	 * Return code is silently ignored.
	 */
	ioctl(vfd, VT_RELDISP, VT_ACKACQ);
}

/* Set the signal masks and handlers. */
static void
mask_signals(void)
{

	static sigset_t sig;
	static struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = SA_RESTART;

	if (o_lock_all) {
		/* handle SIGUSR{1,2}... */
		sa.sa_handler = release_vt;
		sigaction(SIGUSR1, &sa, 0);
		sa.sa_handler = acquire_vt;
		sigaction(SIGUSR2, &sa, 0);

		/* ... and ensure they are unblocked. */
		sigemptyset(&sig);
		sigaddset(&sig, SIGUSR1);
		sigaddset(&sig, SIGUSR2);
		sigprocmask(SIG_UNBLOCK, &sig, 0);
	}

	/* Ignore all the rest. */
	sa.sa_handler = SIG_IGN;
	if (!o_lock_all) {
		sigaction(SIGUSR1, &sa, 0);
		sigaction(SIGUSR2, &sa, 0);
	}
	sigaction(SIGHUP, &sa, 0);
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGQUIT, &sa, 0);
	sigaction(SIGPIPE, &sa, 0);
	sigaction(SIGALRM, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGTSTP, &sa, 0);
	sigaction(SIGTTIN, &sa, 0);
	sigaction(SIGTTOU, &sa, 0);
	sigaction(SIGURG, &sa, 0);
	sigaction(SIGVTALRM, &sa, 0);
	sigaction(SIGIO, &sa, 0);
	sigaction(SIGPWR, &sa, 0);

	/*
	 * Also block SIGCHLD.
	 * Not really needed; just make sleep(3) more easy.
	 */
	sigemptyset(&sig);
	sigaddset(&sig, SIGCHLD);
	sigprocmask(SIG_BLOCK, &sig, 0);
}

int init_vt(const char *tty)
{
	const char dev_tty[] = "/dev/tty";

	vfd = open(dev_tty, O_RDWR);
	if (vfd < 0) {
		kbd_warning(errno, "could not open %s", dev_tty);
		return 0;
	}

	/*
	 * First we will set process control of VC switching.
	 * - If this fails, then we know that we aren't on a VC,
	 *   and will print a warning message.
	 * - If it doesn't fail, it gets the current VT status.
	 */
	if (ioctl(vfd, VT_GETMODE, &ovtm) < 0) {
		is_vt = 0;
		fprintf(stderr, _("This tty (%s) is not a virtual console.\n"),
		        tty);
		if (o_lock_all) {
			o_lock_all = 0;
			close(vfd);
			vfd = -1;
			fprintf(stderr,
			        _("The entire console display cannot be locked.\n"));
			return 0;
		}
		fprintf(stderr, "\n\n");
		fflush(stderr);
	} else {
		is_vt = 1;
	}

	/* If we aren't going to lock console, we don't need VT descriptor. */
	if (!o_lock_all) {
		close(vfd);
		vfd = -1;
	}

	mask_signals();

	if (o_lock_all) {
		struct vt_mode vtm = ovtm;

		vtm.mode   = VT_PROCESS; /* Process controls switching. */
		vtm.relsig = SIGUSR1;    /* Signal to raise on release request, handled by release_vt(). */
		vtm.acqsig = SIGUSR2;    /* Signal to raise on acquisition, handled by acquire_vt(). */

		/* Set mode of active vt. */
		if (ioctl(vfd, VT_SETMODE, &vtm) < 0) {
			kbd_warning(errno, "ioctl VT_SETMODE");
			return 0;
		}
	}

	if (is_vt)
		init_screen();

	return 1;
}

void restore_vt(void)
{
	if (is_vt) {
		restore_screen();

		if (o_lock_all) {
			/*
			 * Reset mode of active vt.
			 * Don't check return code - it won't help anyway.
			 */
			ioctl(vfd, VT_SETMODE, &ovtm);
		}
	}
}
