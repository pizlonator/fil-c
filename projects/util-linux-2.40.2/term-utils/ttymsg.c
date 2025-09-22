/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Modified Sun Mar 12 10:39:22 1995, faith@cs.unc.edu for Linux
 *
 */

 /* 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  * Sun Mar 21 1999 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
  * - fixed strerr(errno) in gettext calls
  */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <paths.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nls.h"
#include "closestream.h"
#include "pathnames.h"
#include "ttymsg.h"

#define ERR_BUFLEN	(MAXNAMLEN + 1024)

/*
 * Display the contents of a uio structure on a terminal.  Used by wall(1),
 * syslogd(8), and talkd(8).  Forks and finishes in child if write would block,
 * waiting up to tmout seconds.  Returns pointer to error string on unexpected
 * error; string is not newline-terminated.  Various "normal" errors are
 * ignored (exclusive-use, lack of permission, etc.).
 */
char *
ttymsg(struct iovec *iov, size_t iovcnt, char *line, int tmout) {
	static char device[MAXNAMLEN];
	static char errbuf[ERR_BUFLEN];
	size_t cnt, left;
	ssize_t wret;
	struct iovec localiov[6];
	int fd, forked = 0;
	ssize_t	len = 0;

	if (iovcnt > ARRAY_SIZE(localiov)) {
		snprintf(errbuf, sizeof(errbuf), _("internal error: too many iov's"));
		return errbuf;
	}

	/* The old code here rejected the line argument when it contained a '/',
	   saying: "A slash may be an attempt to break security...".
	   However, if a user can control the line argument here
	   then they can make this routine write to /dev/hda or /dev/sda
	   already. So, this test was worthless, and these days it is
	   also wrong since people use /dev/pts/xxx. */

	len = snprintf(device, sizeof(device), "%s%s", _PATH_DEV, line);
	if (len < 0 || (size_t)len >= sizeof(device)) {
		snprintf(errbuf, sizeof(errbuf), _("excessively long line arg"));
		return errbuf;
	}

	/*
	 * open will fail on slip lines or exclusive-use lines
	 * if not running as root; not an error.
	 */
	if ((fd = open(device, O_WRONLY|O_NONBLOCK, 0)) < 0) {
		if (errno == EBUSY || errno == EACCES || errno == ENOENT)
			return NULL;

		len = snprintf(errbuf, sizeof(errbuf), "%s: %m", device);
		if (len < 0 || (size_t)len >= sizeof(errbuf))
			snprintf(errbuf, sizeof(errbuf), _("open failed"));
		return errbuf;
	}

	for (cnt = left = 0; cnt < iovcnt; ++cnt)
		left += iov[cnt].iov_len;

	for (;;) {
		wret = writev(fd, iov, iovcnt);
		if (wret >= (ssize_t) left)
			break;
		if (wret >= 0) {
			left -= wret;
			if (iov != localiov) {
				memmove(localiov, iov,
				    iovcnt * sizeof(struct iovec));
				iov = localiov;
			}
			for (cnt = 0; wret >= (ssize_t) iov->iov_len && iovcnt > 0; ++cnt) {
				wret -= iov->iov_len;
				++iov;
				--iovcnt;
			}
			if (wret) {
				iov->iov_base = (char *) iov->iov_base + wret;
				iov->iov_len -= wret;
			}
			continue;
		}
		if (errno == EWOULDBLOCK) {
			int cpid, flags;
			sigset_t sigmask;

			if (forked) {
				close(fd);
				_exit(EXIT_FAILURE);
			}
			cpid = fork();
			if (cpid < 0) {
				len = snprintf(errbuf, sizeof(errbuf), _("fork: %m"));
				if (len < 0 || (size_t)len >= sizeof(errbuf))
					snprintf(errbuf, sizeof(errbuf), _("cannot fork"));
				close(fd);
				return errbuf;
			}
			if (cpid) {	/* parent */
				close(fd);
				return NULL;
			}
			forked++;
			/* wait at most tmout seconds */
			signal(SIGALRM, SIG_DFL);
			signal(SIGTERM, SIG_DFL); /* XXX */
			sigemptyset(&sigmask);
			sigprocmask (SIG_SETMASK, &sigmask, NULL);
			alarm((u_int)tmout);
			flags = fcntl(fd, F_GETFL);
			fcntl(flags, F_SETFL, (long) (flags & ~O_NONBLOCK));
			continue;
		}
		/*
		 * We get ENODEV on a slip line if we're running as root,
		 * and EIO if the line just went away.
		 */
		if (errno == ENODEV || errno == EIO)
			break;
		if (close_fd(fd) != 0)
			warn(_("write failed: %s"), device);
		if (forked)
			_exit(EXIT_FAILURE);

		len = snprintf(errbuf, sizeof(errbuf), "%s: %m", device);
		if (len < 0 || (size_t)len >= sizeof(errbuf))
			snprintf(errbuf, sizeof(errbuf),
					_("%s: BAD ERROR, message is "
					  "far too long"), device);
		return errbuf;
	}

	close(fd);

	if (forked)
		_exit(EXIT_SUCCESS);
	return NULL;
}
