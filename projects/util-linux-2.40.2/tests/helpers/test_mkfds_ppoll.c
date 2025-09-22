/*
 * test_mkfds_ppoll.c - call ppoll(2) DIRECTLY
 *
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/* Why this ppoll multiplexer must be defined here?
 *
 * Glibc defines sigset_t its own way. However, it is not what kernel
 * expects.
 *
 * If an applicaiton uses the glibc's sigset_t via ppoll(2) wrapper,
 * there is no problem; the wrapper function may translate the glibc's
 * sigset_t to what kernel expects.
 *
 * Here, we want to ppoll(2) directly. In this case, the glibc's sigset_t
 * becomes an issue. To use ppoll(2) directly, we have to include <asm/poll.h>.
 * Including both <poll.h> provided by glibc and <asm/poll.h> provided by kernel
 * causes "redefined ***" errors.
 *
 * This file is for defining the poll multiplexer only with <asm/poll.h>.
 *
 */
#include "test_mkfds.h"

#include <string.h>		/* memset */
#include <err.h>		/* err */
#include <errno.h>		/* EINTR */
#include <linux/poll.h>		/* struct pollfd */
#include <asm/signal.h>		/* sigset_t */

extern long syscall(long number, ...);
extern void *calloc(size_t nmemb, size_t size);
extern void free(void *ptr);

#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif
#ifndef XALLOC_EXIT_CODE
# define XALLOC_EXIT_CODE EXIT_FAILURE
#endif

/* Copied from include/xalloc.h */
static void *xcalloc(const size_t nelems, const size_t size)
{
	void *ret = calloc(nelems, size);

	if (!ret && size && nelems)
		err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
	return ret;
}

/* sigemptyset may not be defiend and/or declared in asm/signal.h */
static void clear_sigset(sigset_t *sigset)
{
	memset(sigset, 0, sizeof(*sigset));
}

#ifdef __NR_ppoll
DEFUN_WAIT_EVENT_POLL(ppoll,
		      "ppoll",
		      sigset_t sigset;,
		      clear_sigset(&sigset);,
		      syscall(__NR_ppoll, pfds, n, NULL, &sigset, sizeof(sigset)))
#endif
