/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2004-2005, 2007, 2010, 2012-2015, 2017-2018
 *	Todd C. Miller <Todd.Miller@sudo.ws>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#ifdef __linux__
# include <sys/syscall.h>
# if defined(__NR_close_range) && !defined(SYS_close_range)
#  define SYS_close_range __NR_close_range
# endif
#endif

#include <fcntl.h>
#include <unistd.h>

#if defined(__linux__) && defined(SYS_close_range)
static inline int
sys_close_range(unsigned int fd, unsigned int max_fd, unsigned int flags)
{

	return (int)syscall(SYS_close_range, fd, max_fd, flags);
}
#endif

/*
 * Close all file descriptors greater than or equal to lowfd.
 * This is the expensive (fallback) method.
 */
static int
closefrom_fallback(int lowfd)
{
	int fd, maxfd;

#ifdef _SC_OPEN_MAX
	maxfd = (int)sysconf(_SC_OPEN_MAX);
#else
	maxfd = getdtablesize();
#endif
	if (maxfd == -1)
		return -1;

	for (fd = lowfd; fd < maxfd; fd++)
	       close(fd);
	return 0;
}

/*
 *  * Close all file descriptors greater than or equal to lowfd.
 *   * We try the fast way first, falling back on the slow method.
 *    */
void
closefrom(int lowfd)
{

#if defined(__linux__) && defined(SYS_close_range)
	if (sys_close_range((unsigned int)lowfd, UINT_MAX, 0) == 0)
		return;
#endif

	closefrom_fallback(lowfd);
}
